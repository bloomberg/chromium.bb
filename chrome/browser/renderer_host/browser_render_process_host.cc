// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "chrome/browser/renderer_host/browser_render_process_host.h"

#include "build/build_config.h"

#include <algorithm>
#include <limits>
#include <vector>

#if defined(OS_POSIX)
#include <utility>  // for pair<>
#endif

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/field_trial.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/visitedlink_master.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/result_codes.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_thread.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_switches.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "chrome/browser/sandbox_policy.h"
#elif defined(OS_LINUX)
#include "base/singleton.h"
#include "chrome/browser/zygote_host_linux.h"
#include "chrome/browser/renderer_host/render_crash_handler_host_linux.h"
#include "chrome/browser/renderer_host/render_sandbox_host_linux.h"
#endif

using WebKit::WebCache;

#include "third_party/skia/include/core/SkBitmap.h"


// This class creates the IO thread for the renderer when running in
// single-process mode.  It's not used in multi-process mode.
class RendererMainThread : public base::Thread {
 public:
  explicit RendererMainThread(const std::string& channel_id)
      : base::Thread("Chrome_InProcRendererThread"),
        channel_id_(channel_id),
        render_process_(NULL) {
  }

  ~RendererMainThread() {
    Stop();
  }

 protected:
  virtual void Init() {
#if defined(OS_WIN)
    CoInitialize(NULL);
#endif

    render_process_ = new RenderProcess();
    render_process_->set_main_thread(new RenderThread(channel_id_));
    // It's a little lame to manually set this flag.  But the single process
    // RendererThread will receive the WM_QUIT.  We don't need to assert on
    // this thread, so just force the flag manually.
    // If we want to avoid this, we could create the InProcRendererThread
    // directly with _beginthreadex() rather than using the Thread class.
    base::Thread::SetThreadWasQuitProperly(true);
  }

  virtual void CleanUp() {
    delete render_process_;

#if defined(OS_WIN)
    CoUninitialize();
#endif
  }

 private:
  std::string channel_id_;
  // Deleted in CleanUp() on the renderer thread, so don't use a smart pointer.
  RenderProcess* render_process_;
};


// Size of the buffer after which individual link updates deemed not warranted
// and the overall update should be used instead.
static const unsigned kVisitedLinkBufferThreshold = 50;

// This class manages buffering and sending visited link hashes (fingerprints)
// to renderer based on widget visibility.
// As opposed to the VisitedLinkEventListener in profile.cc, which coalesces to
// reduce the rate of messages being sent to render processes, this class
// ensures that the updates occur only when explicitly requested. This is
// used by BrowserRenderProcessHost to only send Add/Reset link events to the
// renderers when their tabs are visible and the corresponding RenderViews are
// created.
class VisitedLinkUpdater {
 public:
  VisitedLinkUpdater() : reset_needed_(false), has_receiver_(false) {}

  // Buffers |links| to update, but doesn't actually relay them.
  void AddLinks(const VisitedLinkCommon::Fingerprints& links) {
    if (reset_needed_)
      return;

    if (pending_.size() + links.size() > kVisitedLinkBufferThreshold) {
      // Once the threshold is reached, there's no need to store pending visited
      // link updates -- we opt for resetting the state for all links.
      AddReset();
      return;
    }

    pending_.insert(pending_.end(), links.begin(), links.end());
  }

  // Tells the updater that sending individual link updates is no longer
  // necessary and the visited state for all links should be reset.
  void AddReset() {
    reset_needed_ = true;
    pending_.clear();
  }

  // Sends visited link update messages: a list of links whose visited state
  // changed or reset of visited state for all links.
  void Update(IPC::Channel::Sender* sender) {
    DCHECK(sender);

    if (!has_receiver_)
      return;

    if (reset_needed_) {
      sender->Send(new ViewMsg_VisitedLink_Reset());
      reset_needed_ = false;
      return;
    }

    if (pending_.size() == 0)
      return;

    sender->Send(new ViewMsg_VisitedLink_Add(pending_));

    pending_.clear();
  }

  // Notifies the updater that it is now safe to send visited state updates.
  void ReceiverReady(IPC::Channel::Sender* sender) {
    has_receiver_ = true;
    // Go ahead and send whatever we already have buffered up.
    Update(sender);
  }

 private:
  bool reset_needed_;
  bool has_receiver_;
  VisitedLinkCommon::Fingerprints pending_;
};

BrowserRenderProcessHost::BrowserRenderProcessHost(Profile* profile)
    : RenderProcessHost(profile),
      visible_widgets_(0),
      backgrounded_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(cached_dibs_cleaner_(
            base::TimeDelta::FromSeconds(5),
            this, &BrowserRenderProcessHost::ClearTransportDIBCache)),
      zygote_child_(false) {
  widget_helper_ = new RenderWidgetHelper();

  registrar_.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                 NotificationService::AllSources());
  visited_link_updater_.reset(new VisitedLinkUpdater());

  WebCacheManager::GetInstance()->Add(id());
  ChildProcessSecurityPolicy::GetInstance()->Add(id());

  // Note: When we create the BrowserRenderProcessHost, it's technically
  //       backgrounded, because it has no visible listeners.  But the process
  //       doesn't actually exist yet, so we'll Background it later, after
  //       creation.
}

BrowserRenderProcessHost::~BrowserRenderProcessHost() {
  WebCacheManager::GetInstance()->Remove(id());
  ChildProcessSecurityPolicy::GetInstance()->Remove(id());

  // We may have some unsent messages at this point, but that's OK.
  channel_.reset();

  // Destroy the AudioRendererHost properly.
  if (audio_renderer_host_.get())
    audio_renderer_host_->Destroy();

  if (process_.handle() && !run_renderer_in_process()) {
    if (zygote_child_) {
#if defined(OS_LINUX)
      Singleton<ZygoteHost>()->EnsureProcessTerminated(process_.handle());
#endif
    } else {
      ProcessWatcher::EnsureProcessTerminated(process_.handle());
    }
  }

  ClearTransportDIBCache();

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_PORT_DELETED_DEBUG,
      Source<IPC::Message::Sender>(this),
      NotificationService::NoDetails());
}

bool BrowserRenderProcessHost::Init() {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (channel_.get())
    return true;

  // run the IPC channel on the shared IO thread.
  base::Thread* io_thread = g_browser_process->io_thread();

  // Construct the AudioRendererHost with the IO thread.
  audio_renderer_host_ =
      new AudioRendererHost(io_thread->message_loop());

  scoped_refptr<ResourceMessageFilter> resource_message_filter =
      new ResourceMessageFilter(g_browser_process->resource_dispatcher_host(),
                                id(),
                                audio_renderer_host_.get(),
                                PluginService::GetInstance(),
                                g_browser_process->print_job_manager(),
                                profile(),
                                widget_helper_,
                                profile()->GetSpellChecker());

  // Find the renderer before creating the channel so if this fails early we
  // return without creating the channel.
  FilePath renderer_path = ChildProcessHost::GetChildPath();
  if (renderer_path.empty())
    return false;

  // Setup the IPC channel.
  const std::string channel_id =
      ChildProcessInfo::GenerateRandomChannelID(this);
  channel_.reset(
      new IPC::SyncChannel(channel_id, IPC::Channel::MODE_SERVER, this,
                           resource_message_filter,
                           io_thread->message_loop(), true,
                           g_browser_process->shutdown_event()));
  // As a preventive mesure, we DCHECK if someone sends a synchronous message
  // with no time-out, which in the context of the browser process we should not
  // be doing.
  channel_->set_sync_messages_with_no_timeout_allowed(false);

  // Build command line for renderer, we have to quote the executable name to
  // deal with spaces.
  CommandLine cmd_line(renderer_path);
  cmd_line.AppendSwitchWithValue(switches::kProcessChannelID,
                                 ASCIIToWide(channel_id));
  bool has_cmd_prefix;
  AppendRendererCommandLine(&cmd_line, &has_cmd_prefix);

  if (run_renderer_in_process()) {
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  Since the primordial
    // thread in the renderer process runs the WebKit code and can sometimes
    // make blocking calls to the UI thread (i.e. this thread), they need to run
    // on separate threads.
    in_process_renderer_.reset(new RendererMainThread(channel_id));

    base::Thread::Options options;
#if !defined(OS_LINUX)
    // In-process plugins require this to be a UI message loop.
    options.message_loop_type = MessageLoop::TYPE_UI;
#else
    // We can't have multiple UI loops on Linux, so we don't support
    // in-process plugins.
    options.message_loop_type = MessageLoop::TYPE_DEFAULT;
#endif
    in_process_renderer_->StartWithOptions(options);
  } else {
    base::TimeTicks begin_launch_time = base::TimeTicks::Now();

    // Actually spawn the child process.
    base::ProcessHandle process = ExecuteRenderer(&cmd_line, has_cmd_prefix);
    if (!process) {
      channel_.reset();
      return false;
    }
    process_.set_handle(process);
    fast_shutdown_started_ = false;

    // Log the launch time, separating out the first one (which will likely be
    // slower due to the rest of the browser initializing at the same time).
    static bool done_first_launch = false;
    if (done_first_launch) {
      UMA_HISTOGRAM_TIMES("MPArch.RendererLaunchSubsequent",
                          base::TimeTicks::Now() - begin_launch_time);
    } else {
      UMA_HISTOGRAM_TIMES("MPArch.RendererLaunchFirst",
                          base::TimeTicks::Now() - begin_launch_time);
      done_first_launch = true;
    }
  }

  resource_message_filter->Init();

  // Now that the process is created, set its backgrounding accordingly.
  SetBackgrounded(backgrounded_);

  InitVisitedLinks();
  InitUserScripts();
  InitExtensions();

  if (max_page_id_ != -1)
    channel_->Send(new ViewMsg_SetNextPageID(max_page_id_ + 1));

  return true;
}

int BrowserRenderProcessHost::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void BrowserRenderProcessHost::CancelResourceRequests(int render_widget_id) {
  widget_helper_->CancelResourceRequests(render_widget_id);
}

void BrowserRenderProcessHost::CrossSiteClosePageACK(
    const ViewMsg_ClosePage_Params& params) {
  widget_helper_->CrossSiteClosePageACK(params);
}

bool BrowserRenderProcessHost::WaitForPaintMsg(int render_widget_id,
                                               const base::TimeDelta& max_delay,
                                               IPC::Message* msg) {
  return widget_helper_->WaitForPaintMsg(render_widget_id, max_delay, msg);
}

void BrowserRenderProcessHost::ReceivedBadMessage(uint16 msg_type) {
  BadMessageTerminateProcess(msg_type, process_.handle());
}

void BrowserRenderProcessHost::ViewCreated() {
  visited_link_updater_->ReceiverReady(this);
}

void BrowserRenderProcessHost::WidgetRestored() {
  // Verify we were properly backgrounded.
  DCHECK_EQ(backgrounded_, (visible_widgets_ == 0));
  visible_widgets_++;
  visited_link_updater_->Update(this);
  SetBackgrounded(false);
}

void BrowserRenderProcessHost::WidgetHidden() {
  // On startup, the browser will call Hide
  if (backgrounded_)
    return;

  DCHECK_EQ(backgrounded_, (visible_widgets_ == 0));
  visible_widgets_--;
  DCHECK_GE(visible_widgets_, 0);
  if (visible_widgets_ == 0) {
    DCHECK(!backgrounded_);
    SetBackgrounded(true);
  }
}

void BrowserRenderProcessHost::AddWord(const std::wstring& word) {
  base::Thread* io_thread = g_browser_process->io_thread();
  SpellChecker* spellchecker = profile()->GetSpellChecker();
  if (spellchecker) {
    io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        spellchecker, &SpellChecker::AddWord, word));
  }
}

void BrowserRenderProcessHost::AddVisitedLinks(
    const VisitedLinkCommon::Fingerprints& links) {
  visited_link_updater_->AddLinks(links);
  if (visible_widgets_ == 0)
    return;

  visited_link_updater_->Update(this);
}

void BrowserRenderProcessHost::ResetVisitedLinks() {
  visited_link_updater_->AddReset();
  if (visible_widgets_ == 0)
    return;

  visited_link_updater_->Update(this);
}

void BrowserRenderProcessHost::AppendRendererCommandLine(
    CommandLine* command_line,
    bool* has_cmd_prefix) const {
  if (logging::DialogsAreSuppressed())
    command_line->AppendSwitch(switches::kNoErrorDialogs);

  // Pass the process type first, so it shows first in process listings.
  command_line->AppendSwitchWithValue(switches::kProcessType,
                                      switches::kRendererProcess);

  // Now send any options from our own command line we want to propogate.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  PropogateBrowserCommandLineToRenderer(browser_command_line, command_line);

  // Pass on the browser locale.
  const std::string locale = g_browser_process->GetApplicationLocale();
  command_line->AppendSwitchWithValue(switches::kLang, ASCIIToWide(locale));

  // If we run FieldTrials, we want to pass to their state to the renderer so
  // that it can act in accordance with each state, or record histograms
  // relating to the FieldTrial states.
  std::string field_trial_states;
  FieldTrialList::StatesToString(&field_trial_states);
  if (!field_trial_states.empty()) {
    command_line->AppendSwitchWithValue(switches::kForceFieldTestNameAndValue,
        ASCIIToWide(field_trial_states));
  }

  // A command prefix is something prepended to the command line of the spawned
  // process. It is supported only on POSIX systems.
#if defined(OS_POSIX)
  *has_cmd_prefix =
      browser_command_line.HasSwitch(switches::kRendererCmdPrefix);
  if (*has_cmd_prefix) {
    // launch the renderer child with some prefix (usually "gdb --args")
    const std::wstring prefix =
        browser_command_line.GetSwitchValue(switches::kRendererCmdPrefix);
    command_line->PrependWrapper(prefix);
  }
#else
  *has_cmd_prefix = false;
#endif  // defined(OS_POSIX)

  ChildProcessHost::SetCrashReporterCommandLine(command_line);

  const std::wstring& profile_path =
      browser_command_line.GetSwitchValue(switches::kUserDataDir);
  if (!profile_path.empty())
    command_line->AppendSwitchWithValue(switches::kUserDataDir, profile_path);
}

void BrowserRenderProcessHost::PropogateBrowserCommandLineToRenderer(
    const CommandLine& browser_cmd,
    CommandLine* renderer_cmd) const {
  // Propagate the following switches to the renderer command line (along
  // with any associated values) if present in the browser command line.
  static const char* const switch_names[] = {
    switches::kRendererAssertTest,
    switches::kRendererCrashTest,
    switches::kRendererStartupDialog,
    switches::kNoSandbox,
    switches::kTestSandbox,
    switches::kEnableSeccompSandbox,
#if !defined (GOOGLE_CHROME_BUILD)
    // This is an unsupported and not fully tested mode, so don't enable it for
    // official Chrome builds.
    switches::kInProcessPlugins,
#endif
    switches::kDomAutomationController,
    switches::kUserAgent,
    switches::kJavaScriptFlags,
    switches::kRecordMode,
    switches::kPlaybackMode,
    switches::kNoJsRandomness,
    switches::kDisableBreakpad,
    switches::kFullMemoryCrashReport,
    switches::kEnableLogging,
    switches::kDumpHistogramsOnExit,
    switches::kDisableLogging,
    switches::kLoggingLevel,
    switches::kDebugPrint,
    switches::kMemoryProfiling,
    switches::kEnableWatchdog,
    switches::kMessageLoopHistogrammer,
    switches::kEnableDCHECK,
    switches::kSilentDumpOnDCHECK,
    switches::kUseLowFragHeapCrt,
    switches::kEnableStatsTable,
    switches::kExperimentalSpellcheckerFeatures,
    switches::kDisableAudio,
    switches::kSimpleDataSource,
    switches::kEnableBenchmarking,
    switches::kInternalNaCl,
    switches::kEnableDatabases,
    switches::kDisableByteRangeSupport,
    switches::kEnableWebSockets,
  };

  for (size_t i = 0; i < arraysize(switch_names); ++i) {
    if (browser_cmd.HasSwitch(switch_names[i])) {
      renderer_cmd->AppendSwitchWithValue(switch_names[i],
          browser_cmd.GetSwitchValue(switch_names[i]));
    }
  }
}

#if defined(OS_WIN)

base::ProcessHandle BrowserRenderProcessHost::ExecuteRenderer(
    CommandLine* cmd_line,
    bool has_cmd_prefix) {
  return sandbox::StartProcess(cmd_line);
}

#elif defined(OS_POSIX)

base::ProcessHandle BrowserRenderProcessHost::ExecuteRenderer(
    CommandLine* cmd_line,
    bool has_cmd_prefix) {
#if defined(OS_LINUX)
  // On Linux, normally spawn processes with zygotes. We can't do this when
  // we're spawning child processes through an external program (i.e. there is a
  // command prefix) like GDB so fall through to the POSIX case then.
  if (!has_cmd_prefix) {
    base::GlobalDescriptors::Mapping mapping;
    const int ipcfd = channel_->GetClientFileDescriptor();
    mapping.push_back(std::pair<uint32_t, int>(kPrimaryIPCChannel, ipcfd));
    const int crash_signal_fd =
        Singleton<RenderCrashHandlerHostLinux>()->GetDeathSignalSocket();
    if (crash_signal_fd >= 0) {
      mapping.push_back(std::pair<uint32_t, int>(kCrashDumpSignal,
                                                 crash_signal_fd));
    }
    zygote_child_ = true;
    return Singleton<ZygoteHost>()->ForkRenderer(cmd_line->argv(), mapping);
  }
#endif  // defined(OS_LINUX)

  // NOTE: This code is duplicated with plugin_process_host.cc, but
  // there's not a good place to de-duplicate it.
  base::file_handle_mapping_vector fds_to_map;
  const int ipcfd = channel_->GetClientFileDescriptor();
  fds_to_map.push_back(std::make_pair(ipcfd, kPrimaryIPCChannel + 3));

#if defined(OS_LINUX)
  // On Linux, we need to add some extra file descriptors for crash handling and
  // the sandbox.
  const int crash_signal_fd =
      Singleton<RenderCrashHandlerHostLinux>()->GetDeathSignalSocket();
  if (crash_signal_fd >= 0) {
    fds_to_map.push_back(std::make_pair(crash_signal_fd,
                                        kCrashDumpSignal + 3));
  }
  const int sandbox_fd =
      Singleton<RenderSandboxHostLinux>()->GetRendererSocket();
  fds_to_map.push_back(std::make_pair(sandbox_fd, kSandboxIPCChannel + 3));
#endif  // defined(OS_LINUX)

  // Actually launch the app.
  zygote_child_ = false;
  base::ProcessHandle process_handle;
  if (!base::LaunchApp(cmd_line->argv(), fds_to_map, false, &process_handle))
    return 0;
  return process_handle;
}

#endif  // defined(OS_POSIX)

base::ProcessHandle BrowserRenderProcessHost::GetRendererProcessHandle() {
  if (run_renderer_in_process())
    return base::Process::Current().handle();
  return process_.handle();
}

void BrowserRenderProcessHost::InitVisitedLinks() {
  VisitedLinkMaster* visitedlink_master = profile()->GetVisitedLinkMaster();
  if (!visitedlink_master) {
    return;
  }

  base::SharedMemoryHandle handle_for_process;
  bool r = visitedlink_master->ShareToProcess(GetRendererProcessHandle(),
                                              &handle_for_process);
  DCHECK(r);

  if (base::SharedMemory::IsHandleValid(handle_for_process)) {
    Send(new ViewMsg_VisitedLink_NewTable(handle_for_process));
  }
}

void BrowserRenderProcessHost::InitUserScripts() {
  UserScriptMaster* user_script_master = profile()->GetUserScriptMaster();
  DCHECK(user_script_master);

  if (!user_script_master->ScriptsReady()) {
    // No scripts ready.  :(
    return;
  }

  // Update the renderer process with the current scripts.
  SendUserScriptsUpdate(user_script_master->GetSharedMemory());
}

void BrowserRenderProcessHost::InitExtensions() {
  // TODO(aa): Should only bother sending these function names if this is an
  // extension process.
  std::vector<std::string> function_names;
  ExtensionFunctionDispatcher::GetAllFunctionNames(&function_names);
  Send(new ViewMsg_Extension_SetFunctionNames(function_names));
}

void BrowserRenderProcessHost::SendUserScriptsUpdate(
    base::SharedMemory *shared_memory) {
  base::SharedMemoryHandle handle_for_process;
  if (!shared_memory->ShareToProcess(GetRendererProcessHandle(),
                                     &handle_for_process)) {
    // This can legitimately fail if the renderer asserts at startup.
    return;
  }

  if (base::SharedMemory::IsHandleValid(handle_for_process)) {
    Send(new ViewMsg_UserScripts_UpdatedScripts(handle_for_process));
  }
}

bool BrowserRenderProcessHost::FastShutdownIfPossible() {
  if (!process_.handle())
    return false;  // Render process is probably crashed.
  if (run_renderer_in_process())
    return false;  // Single process mode can't do fast shutdown.

  // Test if there's an unload listener.
  // NOTE: It's possible that an onunload listener may be installed
  // while we're shutting down, so there's a small race here.  Given that
  // the window is small, it's unlikely that the web page has much
  // state that will be lost by not calling its unload handlers properly.
  if (!sudden_termination_allowed())
    return false;

  // Check for any external tab containers, since they may still be running even
  // though this window closed.
  listeners_iterator iter(ListenersIterator());
  while (!iter.IsAtEnd()) {
    // NOTE: This is a bit dangerous.  We know that for now, listeners are
    // always RenderWidgetHosts.  But in theory, they don't have to be.
    const RenderWidgetHost* widget =
        static_cast<const RenderWidgetHost*>(iter.GetCurrentValue());
    DCHECK(widget);
    if (widget && widget->IsRenderView()) {
      const RenderViewHost* rvh = static_cast<const RenderViewHost*>(widget);
      if (rvh->delegate()->IsExternalTabContainer())
        return false;
    }

    iter.Advance();
  }

  // Otherwise, we're allowed to just terminate the process. Using exit code 0
  // means that UMA won't treat this as a renderer crash.
  process_.Terminate(ResultCodes::NORMAL_EXIT);
  // On POSIX, we must additionally reap the child.
#if defined(OS_POSIX)
  if (zygote_child_) {
#if defined(OS_LINUX)
    // If the renderer was created via a zygote, we have to proxy the reaping
    // through the zygote process.
    Singleton<ZygoteHost>()->EnsureProcessTerminated(process_.handle());
#endif  // defined(OS_LINUX)
  } else {
    ProcessWatcher::EnsureProcessGetsReaped(process_.handle());
  }
#endif  // defined(OS_POSIX)
  process_.Close();

  fast_shutdown_started_ = true;
  return true;
}

bool BrowserRenderProcessHost::SendWithTimeout(IPC::Message* msg,
                                               int timeout_ms) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->SendWithTimeout(msg, timeout_ms);
}

// This is a platform specific function for mapping a transport DIB given its id
TransportDIB* BrowserRenderProcessHost::MapTransportDIB(
    TransportDIB::Id dib_id) {
#if defined(OS_WIN)
  // On Windows we need to duplicate the handle from the remote process
  HANDLE section = win_util::GetSectionFromProcess(
      dib_id.handle, GetRendererProcessHandle(), false /* read write */);
  return TransportDIB::Map(section);
#elif defined(OS_MACOSX)
  // On OSX, the browser allocates all DIBs and keeps a file descriptor around
  // for each.
  return widget_helper_->MapTransportDIB(dib_id);
#elif defined(OS_LINUX)
  return TransportDIB::Map(dib_id);
#endif  // defined(OS_LINUX)
}

TransportDIB* BrowserRenderProcessHost::GetTransportDIB(
    TransportDIB::Id dib_id) {
  const std::map<TransportDIB::Id, TransportDIB*>::iterator
      i = cached_dibs_.find(dib_id);
  if (i != cached_dibs_.end()) {
    cached_dibs_cleaner_.Reset();
    return i->second;
  }

  TransportDIB* dib = MapTransportDIB(dib_id);
  if (!dib)
    return NULL;

  if (cached_dibs_.size() >= MAX_MAPPED_TRANSPORT_DIBS) {
    // Clean a single entry from the cache
    std::map<TransportDIB::Id, TransportDIB*>::iterator smallest_iterator;
    size_t smallest_size = std::numeric_limits<size_t>::max();

    for (std::map<TransportDIB::Id, TransportDIB*>::iterator
         i = cached_dibs_.begin(); i != cached_dibs_.end(); ++i) {
      if (i->second->size() <= smallest_size) {
        smallest_iterator = i;
        smallest_size = i->second->size();
      }
    }

    delete smallest_iterator->second;
    cached_dibs_.erase(smallest_iterator);
  }

  cached_dibs_[dib_id] = dib;
  cached_dibs_cleaner_.Reset();
  return dib;
}

void BrowserRenderProcessHost::ClearTransportDIBCache() {
  for (std::map<TransportDIB::Id, TransportDIB*>::iterator
       i = cached_dibs_.begin(); i != cached_dibs_.end(); ++i) {
    delete i->second;
  }

  cached_dibs_.clear();
}

bool BrowserRenderProcessHost::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->Send(msg);
}

void BrowserRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
  mark_child_process_activity_time();
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // dispatch control messages
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(BrowserRenderProcessHost, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_PageContents, OnPageContents)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UpdatedCacheStats,
                          OnUpdatedCacheStats)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SuddenTerminationChanged,
                          SuddenTerminationChanged);
      IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionAddListener,
                          OnExtensionAddListener)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionRemoveListener,
                          OnExtensionRemoveListener)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionCloseChannel,
                          OnExtensionCloseChannel)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP_EX()

    if (!msg_is_ok) {
      // The message had a handler, but its de-serialization failed.
      // We consider this a capital crime. Kill the renderer if we have one.
      ReceivedBadMessage(msg.type());
    }
    return;
  }

  // dispatch incoming messages to the appropriate TabContents
  IPC::Channel::Listener* listener = GetListenerByID(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }
    return;
  }
  listener->OnMessageReceived(msg);
}

void BrowserRenderProcessHost::OnChannelConnected(int32 peer_pid) {
  // process_ is not NULL if we created the renderer process
  if (!process_.handle()) {
    if (fast_shutdown_started_) {
      // We terminated the process, but the ChannelConnected task was still
      // in the queue. We can safely ignore it.
      return;
    } else if (base::GetCurrentProcId() == peer_pid) {
      // We are in single-process mode. In theory we should have access to
      // ourself but it may happen that we don't.
      process_.set_handle(base::GetCurrentProcessHandle());
    } else {
#if defined(OS_WIN)
      // Request MAXIMUM_ALLOWED to match the access a handle
      // returned by CreateProcess() has to the process object.
      process_.set_handle(OpenProcess(MAXIMUM_ALLOWED, FALSE, peer_pid));
#else
      NOTREACHED();
#endif
      DCHECK(process_.handle());
    }
  } else {
    // Need to verify that the peer_pid is actually the process we know, if
    // it is not, we need to panic now. See bug 1002150.
    if (peer_pid != process_.pid()) {
      // This check is invalid on Linux for two reasons:
      //   a) If we are running the renderer in a wrapper (with
      //      --renderer-cmd-prefix) then the we'll see the PID of the wrapper
      //      process, not the renderer itself.
      //   b) If we are using the SUID sandbox with CLONE_NEWPID, then the
      //      renderer will be in a new PID namespace and will believe that
      //      it's PID is 2 or 3.
      // Additionally, this check isn't a security problem on Linux since we
      // don't use the PID as reported by the renderer.
#if !defined(OS_LINUX)
      CHECK(peer_pid == process_.pid()) << peer_pid << " " << process_.pid();
#endif
    }
    mark_child_process_activity_time();
  }

#if defined(IPC_MESSAGE_LOG_ENABLED)
  bool enabled = IPC::Logging::current()->Enabled();
  Send(new ViewMsg_SetIPCLoggingEnabled(enabled));
#endif

}

// Static. This function can be called from any thread.
void BrowserRenderProcessHost::BadMessageTerminateProcess(
    uint16 msg_type, base::ProcessHandle process) {
  LOG(ERROR) << "bad message " << msg_type << " terminating renderer.";
  if (BrowserRenderProcessHost::run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just crash.
    CHECK(false);
  }
  NOTREACHED();
  base::KillProcess(process, ResultCodes::KILLED_BAD_MESSAGE, false);
}

void BrowserRenderProcessHost::OnChannelError() {
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.
  // The OnChannelError notification can fire multiple times due to nested sync
  // calls to a renderer. If we don't have a valid channel here it means we
  // already handled the error.
  if (!channel_.get())
    return;

  bool child_exited;
  bool did_crash;
  if (!process_.handle()) {
    // The process has been terminated (likely FastShutdownIfPossible).
    did_crash = false;
    child_exited = true;
  } else if (zygote_child_) {
#if defined(OS_LINUX)
    did_crash = Singleton<ZygoteHost>()->DidProcessCrash(
        process_.handle(), &child_exited);
#else
    NOTREACHED();
    did_crash = true;
#endif
  } else {
    did_crash = base::DidProcessCrash(&child_exited, process_.handle());
  }

  NotificationService::current()->Notify(
      NotificationType::RENDERER_PROCESS_CLOSED,
      Source<RenderProcessHost>(this),
      Details<bool>(&did_crash));

  // POSIX: If the process crashed, then the kernel closed the socket for it
  // and so the child has already died by the time we get here. Since
  // DidProcessCrash called waitpid with WNOHANG, it'll reap the process.
  // However, if DidProcessCrash didn't reap the child, we'll need to in
  // ~BrowserRenderProcessHost via ProcessWatcher. So we can't close the handle
  // here.
  //
  // This is moot on Windows where |child_exited| will always be true.
  if (child_exited)
    process_.Close();

  WebCacheManager::GetInstance()->Remove(id());

  channel_.reset();

  IDMap<IPC::Channel::Listener>::iterator iter(&listeners_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->OnMessageReceived(
        ViewHostMsg_RenderViewGone(iter.GetCurrentKey()));
    iter.Advance();
  }

  ClearTransportDIBCache();

  // this object is not deleted at this point and may be reused later.
  // TODO(darin): clean this up
}

void BrowserRenderProcessHost::OnPageContents(const GURL& url,
                                              int32 page_id,
                                              const std::wstring& contents) {
  Profile* p = profile();
  if (!p || p->IsOffTheRecord())
    return;

  HistoryService* hs = p->GetHistoryService(Profile::IMPLICIT_ACCESS);
  if (hs)
    hs->SetPageContents(url, contents);
}

void BrowserRenderProcessHost::OnUpdatedCacheStats(
    const WebCache::UsageStats& stats) {
  WebCacheManager::GetInstance()->ObserveStats(id(), stats);
}

void BrowserRenderProcessHost::SuddenTerminationChanged(bool enabled) {
  set_sudden_termination_allowed(enabled);
}

void BrowserRenderProcessHost::SetBackgrounded(bool backgrounded) {
  // If the process_ is NULL, the process hasn't been created yet.
  if (process_.handle()) {
    bool should_set_backgrounded = true;

#if defined(OS_WIN)
    // The cbstext.dll loads as a global GetMessage hook in the browser process
    // and intercepts/unintercepts the kernel32 API SetPriorityClass in a
    // background thread. If the UI thread invokes this API just when it is
    // intercepted the stack is messed up on return from the interceptor
    // which causes random crashes in the browser process. Our hack for now
    // is to not invoke the SetPriorityClass API if the dll is loaded.
    should_set_backgrounded = (GetModuleHandle(L"cbstext.dll") == NULL);
#endif  // OS_WIN

    if (should_set_backgrounded) {
      bool rv = process_.SetProcessBackgrounded(backgrounded);
      if (!rv) {
        return;
      }
    }
  }

  // Note: we always set the backgrounded_ value.  If the process is NULL
  // (and hence hasn't been created yet), we will set the process priority
  // later when we create the process.
  backgrounded_ = backgrounded;
}

// NotificationObserver implementation.
void BrowserRenderProcessHost::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::USER_SCRIPTS_UPDATED: {
      base::SharedMemory* shared_memory =
          Details<base::SharedMemory>(details).ptr();
      if (shared_memory) {
        SendUserScriptsUpdate(shared_memory);
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void BrowserRenderProcessHost::OnExtensionAddListener(
    const std::string& event_name) {
  if (profile()->GetExtensionMessageService()) {
    profile()->GetExtensionMessageService()->AddEventListener(
        event_name, id());
  }
}

void BrowserRenderProcessHost::OnExtensionRemoveListener(
    const std::string& event_name) {
  if (profile()->GetExtensionMessageService()) {
    profile()->GetExtensionMessageService()->RemoveEventListener(
        event_name, id());
  }
}

void BrowserRenderProcessHost::OnExtensionCloseChannel(int port_id) {
  if (profile()->GetExtensionMessageService()) {
    profile()->GetExtensionMessageService()->CloseChannel(port_id);
  }
}
