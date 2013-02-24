// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HUNG_PLUGIN_TAB_HELPER_H_
#define CHROME_BROWSER_UI_HUNG_PLUGIN_TAB_HELPER_H_

#include <map>

#include "base/files/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class InfoBarDelegate;

namespace base {
class FilePath;
}

// Manages per-tab state with regard to hung plugins. This only handles
// Pepper plugins which we know are windowless. Hung NPAPI plugins (which
// may have native windows) can not be handled with infobars and have a
// separate OS-specific hang monitoring.
//
// Our job is:
// - Pop up an infobar when a plugin is hung.
// - Terminate the plugin process if the user so chooses.
// - Periodically re-show the hung plugin infobar if the user closes it without
//   terminating the plugin.
// - Hide the infobar if the plugin starts responding again.
// - Keep track of all of this for any number of plugins.
class HungPluginTabHelper
    : public content::WebContentsObserver,
      public content::NotificationObserver,
      public content::WebContentsUserData<HungPluginTabHelper> {
 public:
  virtual ~HungPluginTabHelper();

  // content::WebContentsObserver overrides:
  virtual void PluginCrashed(const base::FilePath& plugin_path,
                             base::ProcessId plugin_pid) OVERRIDE;
  virtual void PluginHungStatusChanged(int plugin_child_id,
                                       const base::FilePath& plugin_path,
                                       bool is_hung) OVERRIDE;

  // NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called by an infobar when the user selects to kill the plugin.
  void KillPlugin(int child_id);

 private:
  explicit HungPluginTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<HungPluginTabHelper>;

  // Per-plugin state (since there could be more than one plugin hung). The
  // integer key is the child process ID of the plugin process. This maintains
  // the state for all plugins on this page that are currently hung, whether or
  // not we're currently showing the infobar.
  struct PluginState {
    // Initializes the plugin state to be a hung plugin.
    PluginState(const base::FilePath& p, const string16& n);
    ~PluginState();

    base::FilePath path;
    string16 name;

    // Possibly-null if we're not showing an infobar right now.
    InfoBarDelegate* info_bar;

    // Time to delay before re-showing the infobar for a hung plugin. This is
    // increased each time the user cancels it.
    base::TimeDelta next_reshow_delay;

    // Handles calling the helper when the infobar should be re-shown.
    base::Timer timer;

   private:
    // Since the scope of the timer manages our callback, this struct should
    // not be copied.
    DISALLOW_COPY_AND_ASSIGN(PluginState);
  };
  typedef std::map<int, linked_ptr<PluginState> > PluginStateMap;

  // Called on a timer for a hung plugin to re-show the bar.
  void OnReshowTimer(int child_id);

  // Shows the bar for the plugin identified by the given state, updating the
  // state accordingly. The plugin must not have an infobar already.
  void ShowBar(int child_id, PluginState* state);

  // Closes the infobar associated with the given state. Note that this can
  // be called even if the bar is not opened, in which case it will do nothing.
  void CloseBar(PluginState* state);

  content::NotificationRegistrar registrar_;

  // All currently hung plugins.
  PluginStateMap hung_plugins_;

  DISALLOW_COPY_AND_ASSIGN(HungPluginTabHelper);
};

#endif  // CHROME_BROWSER_UI_HUNG_PLUGIN_TAB_HELPER_H_
