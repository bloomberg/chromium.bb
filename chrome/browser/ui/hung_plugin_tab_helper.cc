// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hung_plugin_tab_helper.h"

#include "base/bind.h"
#include "base/process_util.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/result_codes.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_WIN)
#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"
#endif

namespace {

// Delay in seconds before re-showing the hung plugin message. This will be
// increased each time.
const int kInitialReshowDelaySec = 10;

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
      CrashDumpAndTerminateHungChildProcess(data.handle);
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

class HungPluginTabHelper::InfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  InfoBarDelegate(HungPluginTabHelper* helper,
                  InfoBarTabHelper* infobar_helper,
                  int plugin_child_id,
                  const string16& plugin_name);
  virtual ~InfoBarDelegate();

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

HungPluginTabHelper::InfoBarDelegate::InfoBarDelegate(
    HungPluginTabHelper* helper,
    InfoBarTabHelper* infobar_helper,
    int plugin_child_id,
    const string16& plugin_name)
    : ConfirmInfoBarDelegate(infobar_helper),
      helper_(helper),
      plugin_child_id_(plugin_child_id) {
  message_ = l10n_util::GetStringFUTF16(IDS_BROWSER_HANGMONITOR_PLUGIN_INFOBAR,
                                        plugin_name);
  button_text_ = l10n_util::GetStringUTF16(
      IDS_BROWSER_HANGMONITOR_PLUGIN_INFOBAR_KILLBUTTON);
  icon_ = &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_CRASHED);
}

HungPluginTabHelper::InfoBarDelegate::~InfoBarDelegate() {
}

gfx::Image* HungPluginTabHelper::InfoBarDelegate::GetIcon() const {
  return icon_;
}

string16 HungPluginTabHelper::InfoBarDelegate::GetMessageText() const {
  return message_;
}

int HungPluginTabHelper::InfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 HungPluginTabHelper::InfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return button_text_;
}

bool HungPluginTabHelper::InfoBarDelegate::Accept() {
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
  InfoBarTabHelper* infobar_helper = GetInfoBarHelper();
  if (!infobar_helper)
    return;

  // For now, just do a brute-force search to see if we have this plugin. Since
  // we'll normally have 0 or 1, this is fast.
  for (PluginStateMap::iterator i = hung_plugins_.begin();
       i != hung_plugins_.end(); ++i) {
    if (i->second->path == plugin_path) {
      if (i->second->info_bar)
        infobar_helper->RemoveInfoBar(i->second->info_bar);
      hung_plugins_.erase(i);
      break;
    }
  }
}

void HungPluginTabHelper::PluginHungStatusChanged(int plugin_child_id,
                                                  const FilePath& plugin_path,
                                                  bool is_hung) {
  InfoBarTabHelper* infobar_helper = GetInfoBarHelper();
  if (!infobar_helper)
    return;

  PluginStateMap::iterator found = hung_plugins_.find(plugin_child_id);
  if (found != hung_plugins_.end()) {
    if (!is_hung) {
      // Hung plugin became un-hung, close the infobar and delete our info.
      if (found->second->info_bar)
        infobar_helper->RemoveInfoBar(found->second->info_bar);
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
  ::InfoBarDelegate* delegate =
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
  InfoBarTabHelper* infobar_helper = GetInfoBarHelper();
  if (!infobar_helper)
    return;

  DCHECK(!state->info_bar);
  state->info_bar = new InfoBarDelegate(this, infobar_helper,
                                        child_id, state->name);
  infobar_helper->AddInfoBar(state->info_bar);
}

void HungPluginTabHelper::CloseBar(PluginState* state) {
  InfoBarTabHelper* infobar_helper = GetInfoBarHelper();
  if (!infobar_helper)
    return;

  if (state->info_bar) {
    infobar_helper->RemoveInfoBar(state->info_bar);
    state->info_bar = NULL;
  }
}

InfoBarTabHelper* HungPluginTabHelper::GetInfoBarHelper() {
  TabContents* tab_contents = TabContents::FromWebContents(web_contents());
  if (!tab_contents)
    return NULL;
  return tab_contents->infobar_tab_helper();
}
