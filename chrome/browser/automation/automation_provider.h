// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements a browser-side endpoint for UI automation activity.
// The client-side endpoint is implemented by AutomationProxy.
// The entire lifetime of this object should be contained within that of
// the BrowserProcess, and in particular the NotificationService that's
// hung off of it.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string16.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/content_settings.h"
#include "components/autofill/core/browser/field_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

class AutomationBrowserTracker;
class AutomationTabTracker;
class AutomationWindowTracker;
class Browser;
class ExternalTabContainer;
class FindInPageNotificationObserver;
class InitialLoadObserver;
class LoginHandler;
class MetricEventDurationObserver;
class NavigationControllerRestoredObserver;
class Profile;
struct AutomationMsg_Find_Params;

namespace IPC {
class ChannelProxy;
}

namespace content {
class NavigationController;
class RenderViewHost;
}

namespace base {
class DictionaryValue;
}

namespace content {
class DownloadItem;
class WebContents;
}

namespace gfx {
class Point;
}

class AutomationProvider
    : public IPC::Listener,
      public IPC::Sender,
      public base::SupportsWeakPtr<AutomationProvider>,
      public base::RefCountedThreadSafe<
          AutomationProvider, content::BrowserThread::DeleteOnUIThread> {
 public:
  explicit AutomationProvider(Profile* profile);

  Profile* profile() const { return profile_; }

  void set_profile(Profile* profile);

  // Initializes a channel for a connection to an AutomationProxy.
  // If channel_id starts with kNamedInterfacePrefix, it will act
  // as a server, create a named IPC socket with channel_id as its
  // path, and will listen on the socket for incoming connections.
  // If channel_id does not, it will act as a client and establish
  // a connection on its primary IPC channel. See ipc/ipc_channel_posix.cc
  // for more information about kPrimaryIPCChannel.
  bool InitializeChannel(const std::string& channel_id) WARN_UNUSED_RESULT;

  virtual IPC::Channel::Mode GetChannelMode(bool use_named_interface);

  // Sets the number of tabs that we expect; when this number of tabs has
  // loaded, an AutomationMsg_InitialLoadsComplete message is sent.
  void SetExpectedTabCount(size_t expected_tabs);

  // Called when the inital set of tabs has finished loading.
  // Call SetExpectedTabCount(0) to set this to true immediately.
  void OnInitialTabLoadsComplete();

  // Called when the chromeos WebUI OOBE/Login is ready.
  void OnOOBEWebuiReady();

  // Checks all of the initial load conditions, then sends the
  // InitialLoadsComplete message over the automation channel.
  void SendInitialLoadMessage();

  // Call this before calling InitializeChannel. If called, send the
  // InitialLoadsComplete message immediately when the automation channel is
  // connected, without waiting for the initial load conditions to be met.
  void DisableInitialLoadObservers();

  // Get the index of a particular NavigationController object
  // in the given parent window.  This method uses
  // TabStrip::GetIndexForNavigationController to get the index.
  int GetIndexForNavigationController(
      const content::NavigationController* controller,
      const Browser* parent) const;

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener implementation.
  virtual void OnChannelConnected(int pid) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  IPC::Message* reply_message_release() {
    IPC::Message* reply_message = reply_message_;
    reply_message_ = NULL;
    return reply_message;
  }

  // Get the DictionaryValue equivalent for a download item. Caller owns the
  // DictionaryValue.
  base::DictionaryValue* GetDictionaryFromDownloadItem(
      const content::DownloadItem* download,
      bool incognito);

 protected:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<AutomationProvider>;
  virtual ~AutomationProvider();

  // Helper function to find the browser window that contains a given
  // NavigationController and activate that tab.
  // Returns the Browser if found.
  Browser* FindAndActivateTab(content::NavigationController* contents);

  // Convert a tab handle into a WebContents. If |tab| is non-NULL a pointer
  // to the tab is also returned. Returns NULL in case of failure or if the tab
  // is not of the WebContents type.
  content::WebContents* GetWebContentsForHandle(
      int handle, content::NavigationController** tab);

  // Returns the protocol version which typically is the module version.
  virtual std::string GetProtocolVersion();

  // Returns the associated view for the tab handle passed in.
  // Returns NULL on failure.
  content::RenderViewHost* GetViewForTab(int tab_handle);

  // Called on IPC message deserialization failure. Prints an error message
  // and closes the IPC channel.
  void OnMessageDeserializationFailure();

  scoped_ptr<AutomationBrowserTracker> browser_tracker_;
  scoped_ptr<InitialLoadObserver> initial_load_observer_;
  scoped_ptr<MetricEventDurationObserver> metric_event_duration_observer_;
  scoped_ptr<AutomationTabTracker> tab_tracker_;
  scoped_ptr<AutomationWindowTracker> window_tracker_;

  Profile* profile_;

  // A pointer to reply message used when we do asynchronous processing in the
  // message handler.
  // TODO(phajdan.jr): Remove |reply_message_|, it is error-prone.
  IPC::Message* reply_message_;

  // Consumer for asynchronous history queries.
  CancelableRequestConsumer consumer_;

  // Sends a find request for a given query.
  void SendFindRequest(
      content::WebContents* web_contents,
      bool with_json,
      const base::string16& search_string,
      bool forward,
      bool match_case,
      bool find_next,
      IPC::Message* reply_message);

  // True iff we should open a new automation IPC channel if it closes.
  bool reinitialize_on_channel_error_;

 private:
  void OnUnhandledMessage(const IPC::Message& message);

  // Clear and reinitialize the automation IPC channel.
  bool ReinitializeChannel();

  void HandleUnused(const IPC::Message& message, int handle);
  void GetFilteredInetHitCount(int* hit_count);

  // Responds to the FindInPage request, retrieves the search query parameters,
  // launches an observer to listen for results and issues a StartFind request.
  void HandleFindRequest(int handle,
                         const AutomationMsg_Find_Params& params,
                         IPC::Message* reply_message);

  void OnSetPageFontSize(int tab_handle, int font_size);

  // Notify the JavaScript engine in the render to change its parameters
  // while performing stress testing. See
  // |ViewHostMsg_JavaScriptStressTestControl_Commands| in render_messages.h
  // for information on the arguments.
  void JavaScriptStressTestControl(int handle, int cmd, int param);

  void BeginTracing(const std::string& category_patterns, bool* success);
  void EndTracing(IPC::Message* reply_message);
  void OnTraceDataCollected(IPC::Message* reply_message,
                            const base::FilePath& path);

  // Uses the specified encoding to override the encoding of the page in the
  // specified tab.
  void OverrideEncoding(int tab_handle,
                        const std::string& encoding_name,
                        bool* success);

  void ReloadAsync(int tab_handle);
  void StopAsync(int tab_handle);

  // Method called by the popup menu tracker when a popup menu is opened.
  void NotifyPopupMenuOpened();

  scoped_ptr<IPC::ChannelProxy> channel_;
  scoped_ptr<FindInPageNotificationObserver> find_in_page_observer_;

  // True iff we should enable observers that check for initial load conditions.
  bool use_initial_load_observers_;

  // True iff connected to an AutomationProxy.
  bool is_connected_;

  // True iff browser finished loading initial set of tabs.
  bool initial_tab_loads_complete_;

  // True iff ChromeOS webui login ui is ready.
  bool login_webui_ready_;

  // ID of automation channel.
  std::string channel_id_;

  DISALLOW_COPY_AND_ASSIGN(AutomationProvider);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_H_
