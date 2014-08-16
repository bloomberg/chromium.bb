// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hung_plugin_tab_helper.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/chrome_version_info.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"
#endif


namespace {

#if defined(OS_WIN)

// OwnedHandleVector ----------------------------------------------------------

class OwnedHandleVector {
 public:
  typedef std::vector<HANDLE> Handles;
  OwnedHandleVector();
  ~OwnedHandleVector();

  Handles* data() { return &data_; }

 private:
  Handles data_;

  DISALLOW_COPY_AND_ASSIGN(OwnedHandleVector);
};

OwnedHandleVector::OwnedHandleVector() {
}

OwnedHandleVector::~OwnedHandleVector() {
  for (Handles::iterator iter = data_.begin(); iter != data_.end(); ++iter)
    ::CloseHandle(*iter);
}


// Helpers --------------------------------------------------------------------

const char kDumpChildProcessesSequenceName[] = "DumpChildProcesses";

void DumpBrowserInBlockingPool() {
  CrashDumpForHangDebugging(::GetCurrentProcess());
}

void DumpRenderersInBlockingPool(OwnedHandleVector* renderer_handles) {
  for (OwnedHandleVector::Handles::const_iterator iter =
           renderer_handles->data()->begin();
       iter != renderer_handles->data()->end(); ++iter) {
    CrashDumpForHangDebugging(*iter);
  }
}

void DumpAndTerminatePluginInBlockingPool(
    base::win::ScopedHandle* plugin_handle) {
  CrashDumpAndTerminateHungChildProcess(plugin_handle->Get());
}

#endif  // defined(OS_WIN)

// Called on the I/O thread to actually kill the plugin with the given child
// ID. We specifically don't want this to be a member function since if the
// user chooses to kill the plugin, we want to kill it even if they close the
// tab first.
//
// Be careful with the child_id. It's supplied by the renderer which might be
// hacked.
void KillPluginOnIOThread(int child_id) {
  content::BrowserChildProcessHostIterator iter(
      content::PROCESS_TYPE_PPAPI_PLUGIN);
  while (!iter.Done()) {
    const content::ChildProcessData& data = iter.GetData();
    if (data.id == child_id) {
#if defined(OS_WIN)
      HANDLE handle = NULL;
      HANDLE current_process = ::GetCurrentProcess();
      ::DuplicateHandle(current_process, data.handle, current_process, &handle,
                        0, FALSE, DUPLICATE_SAME_ACCESS);
      // Run it in blocking pool so that it won't block the I/O thread. Besides,
      // we would like to make sure that it happens after dumping renderers.
      content::BrowserThread::PostBlockingPoolSequencedTask(
          kDumpChildProcessesSequenceName, FROM_HERE,
          base::Bind(&DumpAndTerminatePluginInBlockingPool,
                     base::Owned(new base::win::ScopedHandle(handle))));
#else
      base::KillProcess(data.handle, content::RESULT_CODE_HUNG, false);
#endif
      break;
    }
    ++iter;
  }
  // Ignore the case where we didn't find the plugin, it may have terminated
  // before this function could run.
}

}  // namespace


// HungPluginInfoBarDelegate --------------------------------------------------

class HungPluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a hung plugin infobar and delegate and adds the infobar to
  // |infobar_service|.  Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   HungPluginTabHelper* helper,
                                   int plugin_child_id,
                                   const base::string16& plugin_name);

 private:
  HungPluginInfoBarDelegate(HungPluginTabHelper* helper,
                            int plugin_child_id,
                            const base::string16& plugin_name);
  virtual ~HungPluginInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  HungPluginTabHelper* helper_;
  int plugin_child_id_;

  base::string16 message_;
  base::string16 button_text_;
};

// static
infobars::InfoBar* HungPluginInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    HungPluginTabHelper* helper,
    int plugin_child_id,
    const base::string16& plugin_name) {
  return infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new HungPluginInfoBarDelegate(
          helper, plugin_child_id, plugin_name))));
}

HungPluginInfoBarDelegate::HungPluginInfoBarDelegate(
    HungPluginTabHelper* helper,
    int plugin_child_id,
    const base::string16& plugin_name)
    : ConfirmInfoBarDelegate(),
      helper_(helper),
      plugin_child_id_(plugin_child_id),
      message_(l10n_util::GetStringFUTF16(
          IDS_BROWSER_HANGMONITOR_PLUGIN_INFOBAR, plugin_name)),
      button_text_(l10n_util::GetStringUTF16(
          IDS_BROWSER_HANGMONITOR_PLUGIN_INFOBAR_KILLBUTTON)) {
}

HungPluginInfoBarDelegate::~HungPluginInfoBarDelegate() {
}

int HungPluginInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_PLUGIN_CRASHED;
}

base::string16 HungPluginInfoBarDelegate::GetMessageText() const {
  return message_;
}

int HungPluginInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 HungPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return button_text_;
}

bool HungPluginInfoBarDelegate::Accept() {
  helper_->KillPlugin(plugin_child_id_);
  return true;
}


// HungPluginTabHelper::PluginState -------------------------------------------

// Per-plugin state (since there could be more than one plugin hung).  The
// integer key is the child process ID of the plugin process.  This maintains
// the state for all plugins on this page that are currently hung, whether or
// not we're currently showing the infobar.
struct HungPluginTabHelper::PluginState {
  // Initializes the plugin state to be a hung plugin.
  PluginState(const base::FilePath& p, const base::string16& n);
  ~PluginState();

  base::FilePath path;
  base::string16 name;

  // Possibly-null if we're not showing an infobar right now.
  infobars::InfoBar* infobar;

  // Time to delay before re-showing the infobar for a hung plugin. This is
  // increased each time the user cancels it.
  base::TimeDelta next_reshow_delay;

  // Handles calling the helper when the infobar should be re-shown.
  base::Timer timer;

 private:
  // Initial delay in seconds before re-showing the hung plugin message.
  static const int kInitialReshowDelaySec;

  // Since the scope of the timer manages our callback, this struct should
  // not be copied.
  DISALLOW_COPY_AND_ASSIGN(PluginState);
};

// static
const int HungPluginTabHelper::PluginState::kInitialReshowDelaySec = 10;

HungPluginTabHelper::PluginState::PluginState(const base::FilePath& p,
                                              const base::string16& n)
    : path(p),
      name(n),
      infobar(NULL),
      next_reshow_delay(base::TimeDelta::FromSeconds(kInitialReshowDelaySec)),
      timer(false, false) {
}

HungPluginTabHelper::PluginState::~PluginState() {
}


// HungPluginTabHelper --------------------------------------------------------

DEFINE_WEB_CONTENTS_USER_DATA_KEY(HungPluginTabHelper);

HungPluginTabHelper::HungPluginTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents) {
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                 content::NotificationService::AllSources());
}

HungPluginTabHelper::~HungPluginTabHelper() {
}

void HungPluginTabHelper::PluginCrashed(const base::FilePath& plugin_path,
                                        base::ProcessId plugin_pid) {
  // TODO(brettw) ideally this would take the child process ID. When we do this
  // for NaCl plugins, we'll want to know exactly which process it was since
  // the path won't be useful.
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (!infobar_service)
    return;

  // For now, just do a brute-force search to see if we have this plugin. Since
  // we'll normally have 0 or 1, this is fast.
  for (PluginStateMap::iterator i = hung_plugins_.begin();
       i != hung_plugins_.end(); ++i) {
    if (i->second->path == plugin_path) {
      if (i->second->infobar)
        infobar_service->RemoveInfoBar(i->second->infobar);
      hung_plugins_.erase(i);
      break;
    }
  }
}

void HungPluginTabHelper::PluginHungStatusChanged(
    int plugin_child_id,
    const base::FilePath& plugin_path,
    bool is_hung) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (!infobar_service)
    return;

  PluginStateMap::iterator found = hung_plugins_.find(plugin_child_id);
  if (found != hung_plugins_.end()) {
    if (!is_hung) {
      // Hung plugin became un-hung, close the infobar and delete our info.
      if (found->second->infobar)
        infobar_service->RemoveInfoBar(found->second->infobar);
      hung_plugins_.erase(found);
    }
    return;
  }

  base::string16 plugin_name =
      content::PluginService::GetInstance()->GetPluginDisplayNameByPath(
          plugin_path);

  linked_ptr<PluginState> state(new PluginState(plugin_path, plugin_name));
  hung_plugins_[plugin_child_id] = state;
  ShowBar(plugin_child_id, state.get());
}

void HungPluginTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, type);
  infobars::InfoBar* infobar =
      content::Details<infobars::InfoBar::RemovedDetails>(details)->first;
  for (PluginStateMap::iterator i = hung_plugins_.begin();
       i != hung_plugins_.end(); ++i) {
    PluginState* state = i->second.get();
    if (state->infobar == infobar) {
      state->infobar = NULL;

      // Schedule the timer to re-show the infobar if the plugin continues to be
      // hung.
      state->timer.Start(FROM_HERE, state->next_reshow_delay,
          base::Bind(&HungPluginTabHelper::OnReshowTimer,
                     base::Unretained(this),
                     i->first));

      // Next time we do this, delay it twice as long to avoid being annoying.
      state->next_reshow_delay *= 2;
      return;
    }
  }
}

void HungPluginTabHelper::KillPlugin(int child_id) {
#if defined(OS_WIN)
  // Dump renderers that are sending or receiving pepper messages, in order to
  // diagnose inter-process deadlocks.
  // Only do that on the Canary channel, for 20% of pepper plugin hangs.
  if (base::RandInt(0, 100) < 20) {
    chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
    if (channel == chrome::VersionInfo::CHANNEL_CANARY) {
      scoped_ptr<OwnedHandleVector> renderer_handles(new OwnedHandleVector);
      HANDLE current_process = ::GetCurrentProcess();
      content::RenderProcessHost::iterator renderer_iter =
          content::RenderProcessHost::AllHostsIterator();
      for (; !renderer_iter.IsAtEnd(); renderer_iter.Advance()) {
        content::RenderProcessHost* host = renderer_iter.GetCurrentValue();
        HANDLE handle = NULL;
        ::DuplicateHandle(current_process, host->GetHandle(), current_process,
                          &handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
        renderer_handles->data()->push_back(handle);
      }
      // If there are a lot of renderer processes, it is likely that we will
      // generate too many crash dumps. They might not all be uploaded/recorded
      // due to our crash dump uploading restrictions. So we just don't generate
      // renderer crash dumps in that case.
      if (renderer_handles->data()->size() > 0 &&
          renderer_handles->data()->size() < 4) {
        content::BrowserThread::PostBlockingPoolSequencedTask(
            kDumpChildProcessesSequenceName, FROM_HERE,
            base::Bind(&DumpBrowserInBlockingPool));
        content::BrowserThread::PostBlockingPoolSequencedTask(
            kDumpChildProcessesSequenceName, FROM_HERE,
            base::Bind(&DumpRenderersInBlockingPool,
                       base::Owned(renderer_handles.release())));
      }
    }
  }
#endif

  PluginStateMap::iterator found = hung_plugins_.find(child_id);
  DCHECK(found != hung_plugins_.end());

  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(&KillPluginOnIOThread, child_id));
  CloseBar(found->second.get());
}

void HungPluginTabHelper::OnReshowTimer(int child_id) {
  // The timer should have been cancelled if the record isn't in our map
  // anymore.
  PluginStateMap::iterator found = hung_plugins_.find(child_id);
  DCHECK(found != hung_plugins_.end());
  DCHECK(!found->second->infobar);
  ShowBar(child_id, found->second.get());
}

void HungPluginTabHelper::ShowBar(int child_id, PluginState* state) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (!infobar_service)
    return;

  DCHECK(!state->infobar);
  state->infobar = HungPluginInfoBarDelegate::Create(infobar_service, this,
                                                     child_id, state->name);
}

void HungPluginTabHelper::CloseBar(PluginState* state) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (infobar_service && state->infobar) {
    infobar_service->RemoveInfoBar(state->infobar);
    state->infobar = NULL;
  }
}
