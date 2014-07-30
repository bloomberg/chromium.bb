// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NAVIGATION_OBSERVER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/autocomplete/autocomplete_match.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;
class ShortcutsBackend;

namespace net {
class URLFetcher;
class URLRequestStatus;
}

// Monitors omnibox navigations in order to trigger behaviors that depend on
// successful navigations.
//
// Currently two such behaviors exist:
// (1) For single-word queries where we can't tell if the entry was a search or
//     an intranet hostname, the omnibox opens as a search by default, but this
//     class attempts to open as a URL via an HTTP HEAD request.  If successful,
//     displays an infobar once the search result has also loaded.  See
//     AlternateNavInfoBarDelegate.
// (2) Omnibox navigations that complete successfully are added to the
//     Shortcuts backend.
//
// TODO(pkasting): Probably NOTIFICATION_OMNIBOX_OPENED_URL should disappear and
// everyone who listened to it should be triggered from this class instead.
//
// The memory management of this object is a bit tricky. The OmniboxEditModel
// will create us and be responsible for us until we attach as an observer
// after a pending load starts (it will delete us if this doesn't happen).
// Once this pending load starts, we're responsible for deleting ourselves.
class OmniboxNavigationObserver : public content::NotificationObserver,
                                  public content::WebContentsObserver,
                                  public net::URLFetcherDelegate {
 public:
  enum LoadState {
    LOAD_NOT_SEEN,
    LOAD_PENDING,
    LOAD_COMMITTED,
  };

  OmniboxNavigationObserver(Profile* profile,
                            const base::string16& text,
                            const AutocompleteMatch& match,
                            const AutocompleteMatch& alternate_nav_match);
  virtual ~OmniboxNavigationObserver();

  LoadState load_state() const { return load_state_; }

  // Called directly by OmniboxEditModel when an extension-related navigation
  // occurs.  Such navigations don't trigger an immediate NAV_ENTRY_PENDING and
  // must be handled separately.
  void OnSuccessfulNavigation();

 private:
  enum FetchState {
    FETCH_NOT_COMPLETE,
    FETCH_SUCCEEDED,
    FETCH_FAILED,
  };

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::WebContentsObserver:
  virtual void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Once the load has committed and any URL fetch has completed, this displays
  // the alternate nav infobar if necessary, and deletes |this|.
  void OnAllLoadingFinished();

  const base::string16 text_;
  const AutocompleteMatch match_;
  const AutocompleteMatch alternate_nav_match_;
  scoped_refptr<ShortcutsBackend> shortcuts_backend_;  // NULL in incognito.
  scoped_ptr<net::URLFetcher> fetcher_;
  LoadState load_state_;
  FetchState fetch_state_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxNavigationObserver);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NAVIGATION_OBSERVER_H_
