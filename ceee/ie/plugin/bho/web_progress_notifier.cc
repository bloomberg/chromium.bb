// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Web progress notifier implementation.
#include "ceee/ie/plugin/bho/web_progress_notifier.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/plugin/bho/dom_utils.h"

namespace {

// In milliseconds. It defines the "effective period" of user action. A user
// action is considered as a possible cause of the next navigation if the
// navigation happens in this period.
// This is a number we feel confident of based on past experience.
const int kUserActionTimeThresholdMs = 500;

// String constants for the values of TransitionQualifier.
const char kClientRedirect[] = "client_redirect";
const char kServerRedirect[] = "server_redirect";
const char kForwardBack[] = "forward_back";
const char kRedirectMetaRefresh[] = "redirect_meta_refresh";
const char kRedirectOnLoad[] = "redirect_onload";
const char kRedirectJavaScript[] = "redirect_javascript";

}  // namespace

WebProgressNotifier::WebProgressNotifier()
    : web_browser_events_source_(NULL),
      main_frame_info_(PageTransition::LINK, 0, kMainFrameId),
      tab_handle_(NULL),
      next_subframe_id_(1),
      main_frame_document_complete_(true),
      tracking_content_window_action_(false),
      tracking_browser_ui_action_(false),
      has_potential_javascript_redirect_(false),
      cached_webrequest_notifier_(NULL),
      webrequest_notifier_initialized_(false),
      create_thread_id_(::GetCurrentThreadId()) {
}

WebProgressNotifier::~WebProgressNotifier() {
  DCHECK(!webrequest_notifier_initialized_);
  DCHECK(web_browser_events_source_ == NULL);
  DCHECK(window_message_source_ == NULL);
  DCHECK(main_browser_ == NULL);
  DCHECK(travel_log_ == NULL);
  DCHECK(tab_handle_ == NULL);
}

HRESULT WebProgressNotifier::Initialize(
    WebBrowserEventsSource* web_browser_events_source,
    HWND tab_window,
    IWebBrowser2* main_browser) {
  if (web_browser_events_source == NULL || tab_window == NULL ||
      main_browser == NULL) {
    return E_INVALIDARG;
  }

  tab_handle_ = reinterpret_cast<CeeeWindowHandle>(tab_window);
  main_browser_ = main_browser;

  CComQIPtr<IServiceProvider> service_provider(main_browser);
  if (service_provider == NULL ||
      FAILED(service_provider->QueryService(
          SID_STravelLogCursor, IID_ITravelLogStg,
          reinterpret_cast<void**>(&travel_log_))) ||
      travel_log_ == NULL) {
    TearDown();
    return E_FAIL;
  }

  web_browser_events_source_ = web_browser_events_source;
  web_browser_events_source_->RegisterSink(this);

  window_message_source_.reset(CreateWindowMessageSource());
  if (window_message_source_ == NULL) {
    TearDown();
    return E_FAIL;
  }
  window_message_source_->RegisterSink(this);

  if (!webrequest_notifier()->RequestToStart())
    NOTREACHED() << "Failed to start the WebRequestNotifier service.";
  webrequest_notifier_initialized_ = true;
  return S_OK;
}

void WebProgressNotifier::TearDown() {
  if (webrequest_notifier_initialized_) {
    webrequest_notifier()->RequestToStop();
    webrequest_notifier_initialized_ = false;
  }
  if (web_browser_events_source_ != NULL) {
    web_browser_events_source_->UnregisterSink(this);
    web_browser_events_source_ = NULL;
  }
  if (window_message_source_ != NULL) {
    window_message_source_->UnregisterSink(this);
    window_message_source_->TearDown();
    window_message_source_.reset(NULL);
  }

  main_browser_.Release();
  travel_log_.Release();
  tab_handle_ = NULL;
}

void WebProgressNotifier::OnBeforeNavigate(IWebBrowser2* browser, BSTR url) {
  if (browser == NULL || url == NULL)
    return;

  if (FilterOutWebBrowserEvent(browser, FilteringInfo::BEFORE_NAVIGATE))
    return;

  FrameInfo* frame_info = NULL;
  if (!GetFrameInfo(browser, &frame_info))
    return;

  // TODO(yzshen@google.com): add support for requestId.
  HRESULT hr = webnavigation_events_funnel().OnBeforeNavigate(
      tab_handle_, url, frame_info->frame_id, -1, base::Time::Now());
  DCHECK(SUCCEEDED(hr))
      << "Failed to fire the webNavigation.onBeforeNavigate event "
      << com::LogHr(hr);

  if (frame_info->IsMainFrame()) {
    frame_info->ClearTransition();

    // The order in which we set these transitions is **very important.**
    // If there was no DocumentComplete, then there are two likely options:
    // the transition was a JavaScript redirect, or the user navigated to a
    // second page before the first was done loading. We initialize the
    // transition to JavaScript redirect first. If there are other signals such
    // as the user clicked/typed, we'll overwrite this value with the
    // appropriate value.
    if (!main_frame_document_complete_ ||
        wcsncmp(url, L"javascript:", wcslen(L"javascript:")) == 0 ||
        has_potential_javascript_redirect_) {
      frame_info->SetTransition(PageTransition::LINK,
                                CLIENT_REDIRECT | REDIRECT_JAVASCRIPT);
    }

    // Override the transition if there is user action in the tab content window
    // or browser UI.
    if (IsPossibleUserActionInContentWindow()) {
      frame_info->SetTransition(PageTransition::LINK, 0);
    } else if (IsPossibleUserActionInBrowserUI()) {
      frame_info->SetTransition(PageTransition::TYPED, 0);
    }

    // Override the transition if we find some signals that we are more
    // confident about.
    if (InOnLoadEvent(browser)) {
      frame_info->SetTransition(PageTransition::LINK,
                                CLIENT_REDIRECT | REDIRECT_ONLOAD);
    } else if (IsMetaRefresh(browser, url)) {
      frame_info->SetTransition(PageTransition::LINK,
                                CLIENT_REDIRECT | REDIRECT_META_REFRESH);
    }

    // Assume that user actions don't have long-lasting effect: user actions
    // before the current navigation may be the cause of this navigation; but
    // they can not affect any subsequent navigation.
    //
    // Under this assumption, we don't need to remember previous user actions.
    tracking_content_window_action_ = false;
    tracking_browser_ui_action_ = false;
  }
}

void WebProgressNotifier::OnDocumentComplete(IWebBrowser2* browser, BSTR url) {
  if (browser == NULL || url == NULL)
    return;

  if (FilterOutWebBrowserEvent(browser, FilteringInfo::DOCUMENT_COMPLETE))
    return;

  FrameInfo* frame_info = NULL;
  if (!GetFrameInfo(browser, &frame_info))
    return;

  if (frame_info->IsMainFrame()) {
    main_frame_document_complete_ = true;

    has_potential_javascript_redirect_ =
        HasPotentialJavaScriptRedirect(browser);
  }

  HRESULT hr = webnavigation_events_funnel().OnCompleted(
      tab_handle_, url, frame_info->frame_id, base::Time::Now());
  DCHECK(SUCCEEDED(hr)) << "Failed to fire the webNavigation.onCompleted event "
                        << com::LogHr(hr);
}

void WebProgressNotifier::OnNavigateComplete(IWebBrowser2* browser, BSTR url) {
  if (browser == NULL || url == NULL)
    return;

  if (FilterOutWebBrowserEvent(browser, FilteringInfo::NAVIGATE_COMPLETE)) {
    filtering_info_.pending_navigate_complete_browser = browser;
    filtering_info_.pending_navigate_complete_url = url;
    filtering_info_.pending_navigate_complete_timestamp = base::Time::Now();
  } else {
    HandleNavigateComplete(browser, url, base::Time::Now());
  }
}

void WebProgressNotifier::HandleNavigateComplete(
    IWebBrowser2* browser,
    BSTR url,
    const base::Time& timestamp) {
  // NOTE: For the first OnNavigateComplete event in a tab/window, this method
  // may not be called at the moment when IE fires the event.
  // As a result, be careful if you need to query the browser state in this
  // method, because the state may have changed after IE fired the event.

  FrameInfo* frame_info = NULL;
  if (!GetFrameInfo(browser, &frame_info))
    return;

  if (frame_info->IsMainFrame()) {
    main_frame_document_complete_ = false;

    if (IsForwardBack(url)) {
      frame_info->SetTransition(PageTransition::AUTO_BOOKMARK, FORWARD_BACK);
    }
  }

  HRESULT hr = webnavigation_events_funnel().OnCommitted(
      tab_handle_, url, frame_info->frame_id,
      PageTransition::CoreTransitionString(frame_info->transition_type),
      TransitionQualifiersString(frame_info->transition_qualifiers).c_str(),
      timestamp);
  DCHECK(SUCCEEDED(hr)) << "Failed to fire the webNavigation.onCommitted event "
                        << com::LogHr(hr);

  if (frame_info->IsMainFrame())
    subframe_map_.clear();
}

void WebProgressNotifier::OnNavigateError(IWebBrowser2* browser, BSTR url,
                                          long status_code) {
  if (browser == NULL || url == NULL)
    return;

  if (FilterOutWebBrowserEvent(browser, FilteringInfo::NAVIGATE_ERROR))
    return;

  FrameInfo* frame_info = NULL;
  if (!GetFrameInfo(browser, &frame_info))
    return;

  HRESULT hr = webnavigation_events_funnel().OnErrorOccurred(
      tab_handle_, url, frame_info->frame_id, CComBSTR(L""), base::Time::Now());
  DCHECK(SUCCEEDED(hr))
      << "Failed to fire the webNavigation.onErrorOccurred event "
      << com::LogHr(hr);
}

void WebProgressNotifier::OnNewWindow(BSTR url_context, BSTR url) {
  if (url_context == NULL || url == NULL)
    return;

  if (FilterOutWebBrowserEvent(NULL, FilteringInfo::NEW_WINDOW))
    return;

  HRESULT hr = webnavigation_events_funnel().OnBeforeRetarget(
      tab_handle_, url_context, url, base::Time::Now());
  DCHECK(SUCCEEDED(hr))
      << "Failed to fire the webNavigation.onBeforeRetarget event "
      << com::LogHr(hr);
}

void WebProgressNotifier::OnHandleMessage(
    WindowMessageSource::MessageType type,
    const MSG* message_info) {
  DCHECK(create_thread_id_ == ::GetCurrentThreadId());

  // This is called when a user input message is about to be handled by any
  // window procedure on the current thread, we should not do anything expensive
  // here that would degrade user experience.
  switch (type) {
    case WindowMessageSource::TAB_CONTENT_WINDOW: {
      if (IsUserActionMessage(message_info->message)) {
        tracking_content_window_action_ = true;
        last_content_window_action_time_ = base::Time::Now();
      }
      break;
    }
    case WindowMessageSource::BROWSER_UI_SAME_THREAD: {
      if (IsUserActionMessage(message_info->message)) {
        tracking_browser_ui_action_ = true;
        last_browser_ui_action_time_ = base::Time::Now();
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

WindowMessageSource* WebProgressNotifier::CreateWindowMessageSource() {
  scoped_ptr<WindowMessageSource> source(new WindowMessageSource());

  return source->Initialize() ? source.release() : NULL;
}

std::string WebProgressNotifier::TransitionQualifiersString(
    TransitionQualifiers qualifiers) {
  std::string result;
  for (unsigned int current_qualifier = FIRST_TRANSITION_QUALIFIER;
       current_qualifier <= LAST_TRANSITION_QUALIFIER;
       current_qualifier = current_qualifier << 1) {
    if ((qualifiers & current_qualifier) != 0) {
      if (!result.empty())
        result.append("|");
      switch (current_qualifier) {
        case CLIENT_REDIRECT:
          result.append(kClientRedirect);
          break;
        case SERVER_REDIRECT:
          result.append(kServerRedirect);
          break;
        case FORWARD_BACK:
          result.append(kForwardBack);
          break;
        case REDIRECT_META_REFRESH:
          result.append(kRedirectMetaRefresh);
          break;
        case REDIRECT_ONLOAD:
          result.append(kRedirectOnLoad);
          break;
        case REDIRECT_JAVASCRIPT:
          result.append(kRedirectJavaScript);
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }
  return result;
}

bool WebProgressNotifier::GetFrameInfo(IWebBrowser2* browser,
                                       FrameInfo** frame_info) {
  DCHECK(browser != NULL && frame_info != NULL);

  if (IsMainFrame(browser)) {
    *frame_info = &main_frame_info_;
    return true;
  }

  CComPtr<IUnknown> browser_identity;
  HRESULT hr = browser->QueryInterface(&browser_identity);
  DCHECK(SUCCEEDED(hr));
  if (FAILED(hr))
    return false;

  SubframeMap::iterator iter = subframe_map_.find(browser_identity);
  if (iter != subframe_map_.end()) {
    *frame_info = &iter->second;
  } else {
    // PageTransition::MANUAL_SUBFRAME, as well as transition qualifiers for
    // subframes, is not supported.
    subframe_map_.insert(std::make_pair(browser_identity,
                                        FrameInfo(PageTransition::AUTO_SUBFRAME,
                                                  0, next_subframe_id_++)));
    *frame_info = &subframe_map_[browser_identity];
  }
  return true;
}

bool WebProgressNotifier::GetDocument(IWebBrowser2* browser,
                                      REFIID id,
                                      void** document) {
  DCHECK(browser != NULL && document != NULL);

  CComPtr<IDispatch> document_disp;
  if (FAILED(browser->get_Document(&document_disp)) || document_disp == NULL)
    return false;
  return SUCCEEDED(document_disp->QueryInterface(id, document)) &&
         *document != NULL;
}

bool WebProgressNotifier::IsForwardBack(BSTR url) {
  DWORD length = 0;
  DWORD position = 0;

  if (FAILED(travel_log_->GetCount(TLEF_RELATIVE_BACK | TLEF_RELATIVE_FORE |
                                   TLEF_INCLUDE_UNINVOKEABLE,
                                   &length))) {
    length = -1;
  } else {
    length++;  // Add 1 for the current entry.
  }

  if (FAILED(travel_log_->GetCount(TLEF_RELATIVE_FORE |
                                   TLEF_INCLUDE_UNINVOKEABLE,
                                   &position))) {
    position = -1;
  }

  // Consider this is a forward/back navigation, if:
  // (1) state of the forward/back list has been successfully retrieved, and
  // (2) the length of the forward/back list is not changed, and
  // (3) (a) the current position is not the newest entry of the
  //         forward/back list, or
  //     (b) we are not at the newest entry of the list before the current
  //         navigation and the URL of the newest entry is not changed by the
  //         current navigation.
  bool is_forward_back =
      length != -1 && previous_travel_log_info_.length != -1 &&
      position != -1 && previous_travel_log_info_.position != -1 &&
      length == previous_travel_log_info_.length &&
      (position != 0 ||
       (previous_travel_log_info_.position != 0 &&
        previous_travel_log_info_.newest_url == url));

  previous_travel_log_info_.length = length;
  previous_travel_log_info_.position = position;
  if (position == 0 && !is_forward_back)
    previous_travel_log_info_.newest_url = url;

  return is_forward_back;
}

bool WebProgressNotifier::InOnLoadEvent(IWebBrowser2* browser) {
  DCHECK(browser != NULL);

  CComPtr<IHTMLDocument2> document;
  if (!GetDocument(browser, IID_IHTMLDocument2,
                   reinterpret_cast<void**>(&document))) {
    return false;
  }

  CComPtr<IHTMLWindow2> window;
  if (FAILED(document->get_parentWindow(&window)) || window == NULL)
    return false;

  CComPtr<IHTMLEventObj> event_obj;
  if (FAILED(window->get_event(&event_obj)) || event_obj == NULL)
    return false;

  CComBSTR type;
  if (FAILED(event_obj->get_type(&type)) ||
      wcscmp(com::ToString(type), L"load") != 0)
    return false;
  else
    return true;
}

bool WebProgressNotifier::IsMetaRefresh(IWebBrowser2* browser, BSTR url) {
  DCHECK(browser != NULL && url != NULL);

  CComPtr<IHTMLDocument3> document;
  if (!GetDocument(browser, IID_IHTMLDocument3,
                   reinterpret_cast<void**>(&document))) {
    return false;
  }

  std::wstring dest_url(url);
  StringToLowerASCII(&dest_url);

  static const wchar_t slash[] = { L'/' };
  // IE can add/remove a slash to/from the URL specified in the meta refresh
  // tag. No redirect occurs as a result of this URL change, so we just compare
  // without slashes here.
  TrimString(dest_url, slash, &dest_url);

  CComPtr<IHTMLElementCollection> meta_elements;
  long length = 0;
  if (FAILED(DomUtils::GetElementsByTagName(document, CComBSTR(L"meta"),
                                            &meta_elements, &length))) {
    return false;
  }

  for (long index = 0; index < length; ++index) {
    CComPtr<IHTMLMetaElement> meta_element;
    if (FAILED(DomUtils::GetElementFromCollection(
        meta_elements, index, IID_IHTMLMetaElement,
        reinterpret_cast<void**>(&meta_element)))) {
      continue;
    }

    CComBSTR http_equiv;
    if (FAILED(meta_element->get_httpEquiv(&http_equiv)) ||
        http_equiv == NULL || _wcsicmp(http_equiv, L"refresh") != 0) {
      continue;
    }

    CComBSTR content_bstr;
    if (FAILED(meta_element->get_content(&content_bstr)) ||
        content_bstr == NULL)
      continue;
    std::wstring content(content_bstr);
    StringToLowerASCII(&content);
    size_t pos = content.find(L"url");
    if (pos == std::wstring::npos)
      continue;
    pos = content.find(L"=", pos + 3);
    if (pos == std::wstring::npos)
      continue;

    std::wstring content_url(content.begin() + pos + 1, content.end());
    TrimWhitespace(content_url, TRIM_ALL, &content_url);
    TrimString(content_url, slash, &content_url);

    // It is possible that the meta tag specifies a relative URL.
    if (!content_url.empty() && EndsWith(dest_url, content_url, true))
      return true;
  }
  return false;
}

bool WebProgressNotifier::HasPotentialJavaScriptRedirect(
    IWebBrowser2* browser) {
  DCHECK(browser != NULL);

  CComPtr<IHTMLDocument3> document;
  if (!GetDocument(browser, IID_IHTMLDocument3,
                   reinterpret_cast<void**>(&document))) {
    return false;
  }

  CComPtr<IHTMLElementCollection> script_elements;
  long length = 0;
  if (FAILED(DomUtils::GetElementsByTagName(document, CComBSTR(L"script"),
                                            &script_elements, &length))) {
    return false;
  }

  for (long index = 0; index < length; ++index) {
    CComPtr<IHTMLScriptElement> script_element;
    if (FAILED(DomUtils::GetElementFromCollection(
        script_elements, index, IID_IHTMLScriptElement,
        reinterpret_cast<void**>(&script_element)))) {
      continue;
    }

    CComBSTR text;
    if (FAILED(script_element->get_text(&text)) || text == NULL)
      continue;

    if (wcsstr(text, L"location.href") != NULL ||
        wcsstr(text, L"location.replace") != NULL ||
        wcsstr(text, L"location.assign") != NULL ||
        wcsstr(text, L"location.reload") != NULL) {
      return true;
    }
  }

  return false;
}

bool WebProgressNotifier::IsPossibleUserActionInContentWindow() {
  if (tracking_content_window_action_) {
    base::TimeDelta delta = base::Time::Now() -
                            last_content_window_action_time_;
    if (delta.InMilliseconds() < kUserActionTimeThresholdMs)
      return true;
  }

  return false;
}

bool WebProgressNotifier::IsPossibleUserActionInBrowserUI() {
  if (tracking_browser_ui_action_) {
    base::TimeDelta delta = base::Time::Now() - last_browser_ui_action_time_;
    if (delta.InMilliseconds() < kUserActionTimeThresholdMs)
      return true;
  }

  // TODO(yzshen@google.com): The windows of the browser UI live in
  // different threads or even processes, for example:
  // 1) The menu bar, add-on toolbands, as well as the status bar at the bottom,
  //    live in the same thread as the tab content window.
  // 2) In IE7, the browser frame lives in a different thread other than the one
  //    hosting the tab content window; in IE8, it lives in a different process.
  // 3) Our extension UI (rendered by Chrome) lives in another process.
  // Currently WebProgressNotifier only handles case (1). I need to find out a
  // solution that can effectively handle case (2) and (3).
  return false;
}

bool WebProgressNotifier::FilterOutWebBrowserEvent(IWebBrowser2* browser,
                                                   FilteringInfo::Event event) {
  if (!IsMainFrame(browser)) {
    if (filtering_info_.state == FilteringInfo::SUSPICIOUS_NAVIGATE_COMPLETE) {
      filtering_info_.state = FilteringInfo::END;
      HandleNavigateComplete(
          filtering_info_.pending_navigate_complete_browser,
          filtering_info_.pending_navigate_complete_url,
          filtering_info_.pending_navigate_complete_timestamp);
    }
  } else {
    switch (filtering_info_.state) {
      case FilteringInfo::END: {
        break;
      }
      case FilteringInfo::START: {
        if (event == FilteringInfo::BEFORE_NAVIGATE)
          filtering_info_.state = FilteringInfo::FIRST_BEFORE_NAVIGATE;

        break;
      }
      case FilteringInfo::FIRST_BEFORE_NAVIGATE: {
        if (event == FilteringInfo::BEFORE_NAVIGATE ||
            event == FilteringInfo::NAVIGATE_COMPLETE) {
          filtering_info_.state = FilteringInfo::END;
        } else if (event == FilteringInfo::DOCUMENT_COMPLETE) {
          filtering_info_.state = FilteringInfo::SUSPICIOUS_DOCUMENT_COMPLETE;
          return true;
        }

        break;
      }
      case FilteringInfo::SUSPICIOUS_DOCUMENT_COMPLETE: {
        if (event == FilteringInfo::BEFORE_NAVIGATE) {
          filtering_info_.state = FilteringInfo::END;
        } else if (event == FilteringInfo::NAVIGATE_COMPLETE) {
          filtering_info_.state = FilteringInfo::SUSPICIOUS_NAVIGATE_COMPLETE;
          return true;
        }

        break;
      }
      case FilteringInfo::SUSPICIOUS_NAVIGATE_COMPLETE: {
        if (event == FilteringInfo::NAVIGATE_COMPLETE) {
          filtering_info_.state = FilteringInfo::END;
          // Ignore the pending OnNavigateComplete event.
        } else {
          filtering_info_.state = FilteringInfo::END;
          HandleNavigateComplete(
              filtering_info_.pending_navigate_complete_browser,
              filtering_info_.pending_navigate_complete_url,
              filtering_info_.pending_navigate_complete_timestamp);
        }
        break;
      }
      default: {
        NOTREACHED() << "Unknown state type.";
        break;
      }
    }
  }
  return false;
}
