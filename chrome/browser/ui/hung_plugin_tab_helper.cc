// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hung_plugin_tab_helper.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(HungPluginTabHelper);

namespace {

// Delay in seconds before re-showing the hung plugin message. This will be
// increased each time.
const int kInitialReshowDelaySec = 10;

#if defined(OS_WIN)

const char kDumpChildProcessesSequenceName[] = "DumpChildProcesses";

class OwnedHandleVector {
 public:
  OwnedHandleVector() {}

  ~OwnedHandleVector() {
    for (std::vector<HANDLE>::const_iterator iter = data_.begin();
         iter != data_.end(); ++iter) {
      ::CloseHandle(*iter);
    }
  }

  std::vector<HANDLE>& data() { return data_; }

 private:
  std::vector<HANDLE> data_;

  DISALLOW_COPY_AND_ASSIGN(OwnedHandleVector);
};

void DumpBrowserInBlockingPool() {
  CrashDumpForHangDebugging(::GetCurrentProcess());
}

void DumpRenderersInBlockingPool(OwnedHandleVector* renderer_handles) {
  for (std::vector<HANDLE>::const_iterator iter =
           renderer_handles->data().begin();
       iter != renderer_handles->data().end(); ++iter) {
    CrashDumpForHangDebugging(*iter);
  }
}

void DumpAndTerminatePluginInBlockingPool(
    base::win::ScopedHandle* plugin_handle) {
  CrashDumpAndTerminateHungChildProcess(plugin_handle->Get());
}

#endif

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

class HungPluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  HungPluginInfoBarDelegate(HungPluginTabHelper* helper,
                            InfoBarService* infobar_service,
                            int plugin_child_id,
                            const string16& plugin_name);
  virtual ~HungPluginInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

 private:
  HungPluginTabHelper* helper_;
  int plugin_child_id_;

  string16 message_;
  string16 button_text_;
  gfx::Image* icon_;
};

HungPluginInfoBarDelegate::HungPluginInfoBarDelegate(
    HungPluginTabHelper* helper,
    InfoBarService* infobar_service,
    int plugin_child_id,
    const string16& plugin_name)
    : ConfirmInfoBarDelegate(infobar_service),
      helper_(helper),
      plugin_child_id_(plugin_child_id) {
  message_ = l10n_util::GetStringFUTF16(IDS_BROWSER_HANGMONITOR_PLUGIN_INFOBAR,
                                        plugin_name);
  button_text_ = l10n_util::GetStringUTF16(
      IDS_BROWSER_HANGMONITOR_PLUGIN_INFOBAR_KILLBUTTON);
  icon_ = &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_CRASHED);
}

HungPluginInfoBarDelegate::~HungPluginInfoBarDelegate() {
}

gfx::Image* HungPluginInfoBarDelegate::GetIcon() const {
  return icon_;
}

string16 HungPluginInfoBarDelegate::GetMessageText() const {
  return message_;
}

int HungPluginInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 HungPluginInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  return button_text_;
}

bool HungPluginInfoBarDelegate::Accept() {
  helper_->KillPlugin(plugin_child_id_);
  return true;
}

// -----------------------------------------------------------------------------

HungPluginTabHelper::PluginState::PluginState(const FilePath& p,
                                              const string16& n)
    : path(p),
      name(n),
      info_bar(NULL),
      next_reshow_delay(base::TimeDelta::FromSeconds(kInitialReshowDelaySec)),
      timer(false, false) {
}

HungPluginTabHelper::PluginState::~PluginState() {
}

// -----------------------------------------------------------------------------

HungPluginTabHelper::HungPluginTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents) {
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                 content::NotificationService::AllSources());
}

HungPluginTabHelper::~HungPluginTabHelper() {
}

void HungPluginTabHelper::PluginCrashed(const FilePath& plugin_path) {
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
      if (i->second->info_bar)
        infobar_service->RemoveInfoBar(i->second->info_bar);
      hung_plugins_.erase(i);
      break;
    }
  }
}

void HungPluginTabHelper::PluginHungStatusChanged(int plugin_child_id,
                                                  const FilePath& plugin_path,
                                                  bool is_hung) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (!infobar_service)
    return;

  PluginStateMap::iterator found = hung_plugins_.find(plugin_child_id);
  if (found != hung_plugins_.end()) {
    if (!is_hung) {
      // Hung plugin became un-hung, close the infobar and delete our info.
      if (found->second->info_bar)
        infobar_service->RemoveInfoBar(found->second->info_bar);
      hung_plugins_.erase(found);
    }
    return;
  }

  string16 plugin_name =
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

  // Note: do not dereference. The InfoBarContainer will delete the object when
  // it gets this notification, we only remove our tracking info, if we have
  // any.
  //
  // TODO(pkasting): This comment will be incorrect and should be removed once
  // InfoBars own their delegates.
  InfoBarDelegate* delegate =
      content::Details<InfoBarRemovedDetails>(details)->first;

  for (PluginStateMap::iterator i = hung_plugins_.begin();
       i != hung_plugins_.end(); ++i) {
    PluginState* state = i->second.get();
    if (state->info_bar == delegate) {
      state->info_bar = NULL;

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
        renderer_handles->data().push_back(handle);
      }
      // If there are a lot of renderer processes, it is likely that we will
      // generate too many crash dumps. They might not all be uploaded/recorded
      // due to our crash dump uploading restrictions. So we just don't generate
      // renderer crash dumps in that case.
      if (renderer_handles->data().size() > 0 &&
          renderer_handles->data().size() < 4) {
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
  if (found == hung_plugins_.end()) {
    NOTREACHED();
    return;
  }

  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(&KillPluginOnIOThread, child_id));
  CloseBar(found->second.get());
}

void HungPluginTabHelper::OnReshowTimer(int child_id) {
  PluginStateMap::iterator found = hung_plugins_.find(child_id);
  if (found == hung_plugins_.end() || found->second->info_bar) {
    // The timer should be cancelled if the record isn't in our map anymore.
    NOTREACHED();
    return;
  }
  ShowBar(child_id, found->second.get());
}

void HungPluginTabHelper::ShowBar(int child_id, PluginState* state) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (!infobar_service)
    return;

  DCHECK(!state->info_bar);
  state->info_bar = new HungPluginInfoBarDelegate(this, infobar_service,
                                                  child_id, state->name);
  infobar_service->AddInfoBar(state->info_bar);
}

void HungPluginTabHelper::CloseBar(PluginState* state) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (!infobar_service)
    return;

  if (state->info_bar) {
    infobar_service->RemoveInfoBar(state->info_bar);
    state->info_bar = NULL;
  }
}
