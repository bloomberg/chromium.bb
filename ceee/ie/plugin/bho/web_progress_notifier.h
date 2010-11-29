// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Web progress notifier implementation.
#ifndef CEEE_IE_PLUGIN_BHO_WEB_PROGRESS_NOTIFIER_H_
#define CEEE_IE_PLUGIN_BHO_WEB_PROGRESS_NOTIFIER_H_

#include <atlbase.h>
#include <tlogstg.h>

#include <map>
#include <string>

#include "base/scoped_ptr.h"
#include "ceee/ie/plugin/bho/web_browser_events_source.h"
#include "ceee/ie/plugin/bho/webrequest_notifier.h"
#include "ceee/ie/plugin/bho/window_message_source.h"
#include "chrome/common/page_transition_types.h"

class WebNavigationEventsFunnel;

// WebProgressNotifier sends to the Broker various Web progress events,
// including Web page navigation events and HTTP request/response events.
class WebProgressNotifier : public WebBrowserEventsSource::Sink,
                            public WindowMessageSource::Sink {
 public:
  WebProgressNotifier();
  virtual ~WebProgressNotifier();

  HRESULT Initialize(
      WebBrowserEventsSource* web_browser_events_source,
      IEventSender* client,
      HWND tab_window,
      IWebBrowser2* main_browser);
  void TearDown();

  // @name WebBrowserEventsSource::Sink implementation
  // @{
  virtual void OnBeforeNavigate(IWebBrowser2* browser, BSTR url);
  virtual void OnDocumentComplete(IWebBrowser2* browser, BSTR url);
  virtual void OnNavigateComplete(IWebBrowser2* browser, BSTR url);
  virtual void OnNavigateError(IWebBrowser2* browser, BSTR url,
                               long status_code);
  virtual void OnNewWindow(BSTR url_context, BSTR url);
  // @}

  // @name WindowMessageSource::Sink implementation
  // @{
  virtual void OnHandleMessage(WindowMessageSource::MessageType type,
                               const MSG* message_info);
  // @}

 protected:
  // The main frame ID.
  static const int kMainFrameId = 0;

  // Sometimes IE fires unexpected events, which could possibly corrupt the
  // internal state of WebProgressNotifier and lead to incorrect webNavigation
  // event sequence. Here is a common issue:
  //
  // When a URL is opened in a new tab/window (CTRL + mouse click, or using
  // "_blank" target in <a> element/IHTMLDocument2::open),
  // (1) if the URL results in server-initiated redirect, the event sequence is:
  //     (1-1) OnNewWindow
  //     (1-2) OnBeforeNavigate http://www.originalurl.com/
  //     (1-3) OnDocumentComplete http://www.originalurl.com/
  //     (1-4) OnNavigateComplete http://www.originalurl.com/
  //     (1-5) OnNavigateComplete http://www.redirecturl.com/
  //     (1-6) OnDocumentComplete http://www.redirecturl.com/
  // (2) otherwise, the event sequence is:
  //     (2-1) OnNewWindow
  //     (2-2) OnBeforeNavigate http://www.url.com/
  //     (2-3) OnDocumentComplete http://www.url.com/
  //     (2-4) OnNavigateComplete http://www.url.com/
  //     (2-5) OnDocumentComplete http://www.url.com/
  //     (NOTE: HTTP responses with status code 304 fall into the 2nd category.)
  //
  // Event 1-3, 1-4 and 2-3 are undesired, comparing with the (normal) event
  // sequence of opening a link in the current tab/window.
  // FilteringInfo is used to filter out these events.
  //
  // It is easy to get rid of event 1-3 and 2-3. If we observe an
  // OnDocumentComplete event immediately after OnBeforeNavigate, we know that
  // it should be ignored.
  // However, it is hard to get rid of event 1-4, since we have no way to tell
  // the difference between 1-4 and 2-4, until we get the next event.
  // (At the first glance, we could tell 1-4 from 2-4 by observing
  // INTERNET_STATUS_REDIRECT to see whether the navigation involves
  // server-initiated redirect. However, INTERNET_STATUS_REDIRECT actually
  // happens *after* event 1-4.) As a result, when we receive an
  // OnNavigateComplete event after an undesired OnDocumentComplete event, we
  // have to postpone making the decision of processing it or not until we
  // receive the next event. We process it only if the next event is not another
  // OnNavigateComplete.
  struct FilteringInfo {
    enum State {
      // The tab/window is newly created.
      START,
      // The first OnBeforeNavigate in the main frame has been observed.
      FIRST_BEFORE_NAVIGATE,
      // A suspicious OnDocumentComplete in the main frame has been observed.
      SUSPICIOUS_DOCUMENT_COMPLETE,
      // A suspicious OnNavigateComplete in the main frame has been observed.
      SUSPICIOUS_NAVIGATE_COMPLETE,
      // Filtering has finished.
      END
    };

    enum Event {
      // An OnBeforeNavigate has been fired.
      BEFORE_NAVIGATE,
      // An OnDocumentComplete has been fired.
      DOCUMENT_COMPLETE,
      // An OnNavigateComplete has been fired.
      NAVIGATE_COMPLETE,
      // An OnNavigateError has been fired.
      NAVIGATE_ERROR,
      // An OnNewWindow has been fired.
      NEW_WINDOW
    };

    FilteringInfo() : state(START) {}

    State state;

    // Arguments of a pending OnNavigateComplete event.
    CComBSTR pending_navigate_complete_url;
    CComPtr<IWebBrowser2> pending_navigate_complete_browser;
    base::Time pending_navigate_complete_timestamp;
  };

  // Any transition type can be augmented by qualifiers, which further define
  // the navigation.
  // Transition types could be found in chrome/common/page_transition_types.h.
  // We are not using the qualifiers defined in that file, since we need to
  // define a few IE/FF specific qualifiers for now.
  enum TransitionQualifier {
    // Redirects caused by JavaScript or a meta refresh tag on the page.
    CLIENT_REDIRECT = 0x1,
    // Redirects sent from the server by HTTP headers.
    SERVER_REDIRECT = 0x2,
    // Users use Forward or Back button to navigate among browsing history.
    FORWARD_BACK = 0x4,
    // Client redirects caused by <meta http-equiv="refresh">. (IE/FF specific)
    REDIRECT_META_REFRESH = 0x8,
    // Client redirects happening in JavaScript onload event handler.
    // (IE/FF specific)
    REDIRECT_ONLOAD = 0x10,
    // Non-onload JavaScript redirects. (IE/FF specific)
    REDIRECT_JAVASCRIPT = 0x20,
    FIRST_TRANSITION_QUALIFIER = CLIENT_REDIRECT,
    LAST_TRANSITION_QUALIFIER = REDIRECT_JAVASCRIPT
  };
  // Represents zero or more transition qualifiers.
  typedef unsigned int TransitionQualifiers;

  // Information related to a frame.
  struct FrameInfo {
    FrameInfo() : transition_type(PageTransition::LINK),
                  transition_qualifiers(0),
                  frame_id(-1) {
    }

    FrameInfo(PageTransition::Type in_transition_type,
              TransitionQualifiers in_transition_qualifiers,
              int in_frame_id)
        : transition_type(in_transition_type),
          transition_qualifiers(in_transition_qualifiers),
          frame_id(in_frame_id) {
    }

    // Clears transition type as well as qualifiers.
    void ClearTransition() {
      SetTransition(PageTransition::LINK, 0);
    }

    // Sets transition type and qualifiers.
    void SetTransition(PageTransition::Type type,
                       TransitionQualifiers qualifiers) {
      transition_type = type;
      transition_qualifiers = qualifiers;
    }

    // Appends more transition qualifiers.
    void AppendTransitionQualifiers(TransitionQualifiers qualifiers) {
      transition_qualifiers |= qualifiers;
    }

    // Whether this FrameInfo instance is associated with the main frame.
    bool IsMainFrame() const {
      return frame_id == kMainFrameId;
    }

    // The transition type for the current navigation in this frame.
    PageTransition::Type transition_type;
    // The transition qualifiers for the current navigation in this frame.
    TransitionQualifiers transition_qualifiers;
    // The frame ID.
    const int frame_id;
  };

  // Accessor so that we can mock it in unit tests.
  virtual WebNavigationEventsFunnel* webnavigation_events_funnel();

  // Accessor so that we can mock WebRequestNotifier in unit tests.
  virtual WebRequestNotifier* webrequest_notifier();

  // Unit testing seems to create a WindowMessageSource instance.
  virtual WindowMessageSource* CreateWindowMessageSource();

  // Whether the current navigation is a navigation among the browsing history
  // (forward/back list).
  // The method is made virtual so that we could easily mock it in unit tests.
  virtual bool IsForwardBack(BSTR url);

  // Whether we are currently inside the onload event handler of the page.
  // The method is made virtual so that we could easily mock it in unit tests.
  virtual bool InOnLoadEvent(IWebBrowser2* browser);

  // Whether there is meta refresh tag on the current page.
  // The method is made virtual so that we could easily mock it in unit tests.
  virtual bool IsMetaRefresh(IWebBrowser2* browser, BSTR url);

  // Whether there is JavaScript code on the current page that could possibly
  // cause a navigation.
  // The method is made virtual so that we could easily mock it in unit tests.
  virtual bool HasPotentialJavaScriptRedirect(IWebBrowser2* browser);

  // Whether there is user action in the content window that could possibly
  // cause a navigation.
  // The method is made virtual so that we could easily mock it in unit tests.
  virtual bool IsPossibleUserActionInContentWindow();

  // Whether there is user action in the browser UI that could possibly cause a
  // navigation.
  // The method is made virtual so that we could easily mock it in unit tests.
  virtual bool IsPossibleUserActionInBrowserUI();

  // Converts a set of transition qualifier values into a string.
  std::string TransitionQualifiersString(TransitionQualifiers qualifiers);

  // Whether the IWebBrowser2 interface belongs to the main frame.
  bool IsMainFrame(IWebBrowser2* browser) {
    return browser != NULL && main_browser_.IsEqualObject(browser);
  }

  // Gets the information related to a frame.
  // @param browser The corresponding IWebBrowser2 interface of a frame.
  // @param frame_info An output parameter to return the information. The caller
  //        doesn't take ownership of the returned object. The FrameInfo
  //        instance for the main frame will live as long as the
  //        WebProgressNotifier instance; all FrameInfo instances for subframes
  //        will be deleted when the main frame navigates.
  // @return Whether the operation is successful or not.
  bool GetFrameInfo(IWebBrowser2* browser, FrameInfo** frame_info);

  // Gets the document of the frame.
  // @param browser The corresponding IWebBrowser2 interface of a frame.
  // @param id The IID of the document interface to return.
  // @param document An output parameter to return the interface pointer.
  // @return Whether the operation is successful or not.
  bool GetDocument(IWebBrowser2* browser, REFIID id, void** document);

  // Whether the specified Windows message represents a user action.
  bool IsUserActionMessage(UINT message) {
    return message == WM_LBUTTONUP || message == WM_KEYUP ||
           message == WM_KEYDOWN;
  }

  // Handles OnNavigateComplete events.
  // @param browser The corresponding IWebBrowser2 interface of a frame.
  // @param url The URL that the frame navigated to.
  // @param timestamp The time when the OnNavigateComplete event was fired.
  void HandleNavigateComplete(IWebBrowser2* browser,
                              BSTR url,
                              const base::Time& timestamp);

  // Decides whether to filter out a navigation event.
  // The method may call HandleNavigateComplete to handle delayed
  // OnNavigateComplete events.
  // @param browser The frame in which the navigation event happens.
  // @param event The navigation event.
  // @return Returns true if the event should be ignored.
  bool FilterOutWebBrowserEvent(IWebBrowser2* browser,
                                FilteringInfo::Event event);

  // This class doesn't have ownership of the object that
  // web_browser_events_source_ points to.
  WebBrowserEventsSource* web_browser_events_source_;
  // Publisher of events about Windows message handling.
  scoped_ptr<WindowMessageSource> window_message_source_;

  // The funnel for sending webNavigation events to the broker.
  scoped_ptr<WebNavigationEventsFunnel> webnavigation_events_funnel_;

  // Information related to the main frame.
  FrameInfo main_frame_info_;

  // IWebBrowser2 interface pointer of the main frame.
  CComPtr<IWebBrowser2> main_browser_;
  // ITravelLogStg interface pointer to manage the forward/back list.
  CComPtr<ITravelLogStg> travel_log_;
  // Window handle of the tab.
  CeeeWindowHandle tab_handle_;

  // The ID to assign to the next subframe.
  int next_subframe_id_;
  // Maintains a map from subframes and their corresponding FrameInfo instances.
  typedef std::map<CAdapt<CComPtr<IUnknown> >, FrameInfo> SubframeMap;
  SubframeMap subframe_map_;

  // Information related to the forward/back list.
  struct TravelLogInfo {
    TravelLogInfo() : length(-1), position(-1) {
    }
    // The length of the forward/back list, including the current entry.
    DWORD length;
    // The current position within the forward/back list, defined as the
    // distance between the current entry and the newest entry in the
    // forward/back list. That is, if the current entry is the newest one
    // in the forward/back list then the position is 0.
    DWORD position;
    // The URL of the newest entry in the forward/back list.
    CComBSTR newest_url;
  };
  // The state of the forward/back list before the current navigation.
  TravelLogInfo previous_travel_log_info_;

  // Whether the previous navigation of the main frame reaches DocumentComplete.
  bool main_frame_document_complete_;

  // If tracking_content_window_action_ is true, consider user action in the tab
  // content window as a possible cause for the next navigation.
  bool tracking_content_window_action_;
  // The last time when the user took action in the content window.
  base::Time last_content_window_action_time_;

  // If tracking_browser_ui_action_ is true, consider user action in the browser
  // UI (except the tab content window) as a possible cause for the next
  // navigation.
  bool tracking_browser_ui_action_;
  // The last time when the user took action in the browser UI.
  base::Time last_browser_ui_action_time_;

  // Whether there is JavaScript code on the current page that can possibly
  // cause a navigation.
  bool has_potential_javascript_redirect_;

  // A cached pointer to the singleton object.
  WebRequestNotifier* cached_webrequest_notifier_;
  bool webrequest_notifier_initialized_;

  DWORD create_thread_id_;

  FilteringInfo filtering_info_;
 private:
  DISALLOW_COPY_AND_ASSIGN(WebProgressNotifier);
};

#endif  // CEEE_IE_PLUGIN_BHO_WEB_PROGRESS_NOTIFIER_H_
