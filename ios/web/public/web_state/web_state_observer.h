// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_OBSERVER_H_

#include <string>
#include <vector>

#include "base/macros.h"

class GURL;

namespace web {

struct Credential;
struct FaviconURL;
struct LoadCommittedDetails;
class WebState;
class WebStateImpl;

enum class PageLoadCompletionStatus : bool { SUCCESS = 0, FAILURE = 1 };

// An observer API implemented by classes which are interested in various page
// load events from WebState.
class WebStateObserver {
 public:
  // Key code associated to form events for which the key code is missing or
  // irrelevant.
  static int kInvalidFormKeyCode;

  // Returns the web state associated with this observer.
  WebState* web_state() const { return web_state_; }

  // This method is invoked when a load request is registered.
  virtual void ProvisionalNavigationStarted(const GURL& url) {}

  // This method is invoked when committed navigation items have been pruned.
  virtual void NavigationItemsPruned(size_t pruned_item_count) {}

  // This method is invoked when a navigation item has changed.
  virtual void NavigationItemChanged() {}

  // This method is invoked when a new non-pending navigation item is created.
  // This corresponds to one NavigationManager item being created
  // (in the case of new navigations) or renavigated to (for back/forward
  // navigations).
  virtual void NavigationItemCommitted(
      const LoadCommittedDetails& load_details) {}

  // Called when the current page has started loading.
  virtual void DidStartLoading() {}

  // Called when the current page has stopped loading.
  virtual void DidStopLoading() {}

  // Called when the current page is loaded.
  virtual void PageLoaded(PageLoadCompletionStatus load_completion_status) {}

  // Called when the interstitial is dismissed by the user.
  virtual void InsterstitialDismissed() {}

  // Called on URL hash change events.
  virtual void UrlHashChanged() {}

  // Called on history state change events.
  virtual void HistoryStateChanged() {}

  // Called on form submission. |user_initiated| is true if the user
  // interacted with the page.
  virtual void DocumentSubmitted(const std::string& form_name,
                                 bool user_initiated) {}

  // Called when the user is typing on a form field, with |error| indicating if
  // there is any error when parsing the form field information.
  // |key_code| may be kInvalidFormKeyCode if there is no key code.
  virtual void FormActivityRegistered(const std::string& form_name,
                                      const std::string& field_name,
                                      const std::string& type,
                                      const std::string& value,
                                      int key_code,
                                      bool input_missing) {}

  // Invoked when new favicon URL candidates are received.
  virtual void FaviconUrlUpdated(const std::vector<FaviconURL>& candidates) {}

  // Notifies the observer that the credential manager API was invoked from
  // |source_url| to request a credential from the browser. If |suppress_ui|
  // is true, the browser MUST NOT show any UI to the user. If this means that
  // no credential will be returned to the page, so be it. Otherwise, the
  // browser may show the user any UI that is necessary to get a Credential and
  // return it to the page. |federations| specifies a list of acceptable
  // federation providers. |user_interaction| indicates whether the API was
  // invoked in response to a user interaction. Responses to the page should
  // provide the specified |request_id|.
  virtual void CredentialsRequested(int request_id,
                                    const GURL& source_url,
                                    bool suppress_ui,
                                    const std::vector<std::string>& federations,
                                    bool is_user_initiated) {}

  // Notifies the observer that the credential manager API was invoked from
  // |source_url| to notify the browser that the user signed in. |credential|
  // specifies the credential that was used to sign in. Responses to the page
  // should provide the specified |request_id|.
  virtual void SignedIn(int request_id,
                        const GURL& source_url,
                        const web::Credential& credential) {}

  // Notifies the observer that the credential manager API was invoked from
  // |source_url| to notify the browser that the user signed in without
  // specifying the credential that was used. Responses to the page should
  // provide the specified |request_id|.
  virtual void SignedIn(int request_id, const GURL& source_url) {}

  // Notifies the observer that the credential manager API was invoked from
  // |source_url| to notify the browser that the user signed out. Responses
  // to the page should provide the specified |request_id|.
  virtual void SignedOut(int request_id, const GURL& source_url) {}

  // Notifies the observer that the credential manager API was invoked from
  // |source_url| to notify the browser that the user failed to sign in.
  // |credential| specifies the credential that failed to sign in. Responses
  // to the page should provide the specified |request_id|.
  virtual void SignInFailed(int request_id,
                            const GURL& source_url,
                            const web::Credential& credential) {}

  // Notifies the observer that the credential manager API was invoked from
  // |source_url| to notify the browser that the user failed to sign in without
  // specifying the credential that failed. Responses to the page should provide
  // the specified |request_id|.
  virtual void SignInFailed(int request_id, const GURL& source_url) {}

  // Invoked when the WebState is being destroyed. Gives subclasses a chance
  // to cleanup.
  virtual void WebStateDestroyed() {}

 protected:
  // Use this constructor when the object is tied to a single WebState for
  // its entire lifetime.
  explicit WebStateObserver(WebState* web_state);

  // Use this constructor when the object wants to observe a WebState for
  // part of its lifetime.  It can then call Observe() to start and stop
  // observing.
  WebStateObserver();

  virtual ~WebStateObserver();

  // Start observing a different WebState; used with the default constructor.
  void Observe(WebState* web_state);

 private:
  friend class WebStateImpl;

  // Stops observing the current web state.
  void ResetWebState();

  WebState* web_state_;

  DISALLOW_COPY_AND_ASSIGN(WebStateObserver);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_OBSERVER_H_
