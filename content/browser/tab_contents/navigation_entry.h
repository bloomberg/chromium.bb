// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_ENTRY_H_
#define CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_ENTRY_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/common/content_export.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/page_type.h"
#include "content/public/common/security_style.h"
#include "googleurl/src/gurl.h"
#include "net/base/cert_status_flags.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SiteInstance;

////////////////////////////////////////////////////////////////////////////////
//
// NavigationEntry class
//
// A NavigationEntry is a data structure that captures all the information
// required to recreate a browsing state. This includes some opaque binary
// state as provided by the TabContents as well as some clear text title and
// URL which is used for our user interface.
//
////////////////////////////////////////////////////////////////////////////////
class CONTENT_EXPORT NavigationEntry {
 public:
  // SSL -----------------------------------------------------------------------

  // Collects the SSL information for this NavigationEntry.
  class SSLStatus {
   public:
    // Flags used for the page security content status.
    enum ContentStatusFlags {
      // HTTP page, or HTTPS page with no insecure content.
      NORMAL_CONTENT             = 0,

      // HTTPS page containing "displayed" HTTP resources (e.g. images, CSS).
      DISPLAYED_INSECURE_CONTENT = 1 << 0,

      // HTTPS page containing "executed" HTTP resources (i.e. script).
      // Also currently used for HTTPS page containing broken-HTTPS resources;
      // this is wrong and should be fixed (see comments in
      // SSLPolicy::OnRequestStarted()).
      RAN_INSECURE_CONTENT       = 1 << 1,
    };

    CONTENT_EXPORT SSLStatus();

    bool Equals(const SSLStatus& status) const {
      return security_style_ == status.security_style_ &&
             cert_id_ == status.cert_id_ &&
             cert_status_ == status.cert_status_ &&
             security_bits_ == status.security_bits_ &&
             content_status_ == status.content_status_;
    }

    void set_security_style(content::SecurityStyle security_style) {
      security_style_ = security_style;
    }
    content::SecurityStyle security_style() const {
      return security_style_;
    }

    void set_cert_id(int ssl_cert_id) {
      cert_id_ = ssl_cert_id;
    }
    int cert_id() const {
      return cert_id_;
    }

    void set_cert_status(net::CertStatus ssl_cert_status) {
      cert_status_ = ssl_cert_status;
    }
    net::CertStatus cert_status() const {
      return cert_status_;
    }

    void set_security_bits(int security_bits) {
      security_bits_ = security_bits;
    }
    int security_bits() const {
      return security_bits_;
    }

    void set_displayed_insecure_content() {
      content_status_ |= DISPLAYED_INSECURE_CONTENT;
    }
    bool displayed_insecure_content() const {
      return (content_status_ & DISPLAYED_INSECURE_CONTENT) != 0;
    }

    void set_ran_insecure_content() {
      content_status_ |= RAN_INSECURE_CONTENT;
    }
    bool ran_insecure_content() const {
      return (content_status_ & RAN_INSECURE_CONTENT) != 0;
    }

    void set_connection_status(int connection_status) {
        connection_status_ = connection_status;
    }
    int connection_status() const {
      return connection_status_;
    }

    // Raw accessors for all the content status flags. This contains a
    // combination of any of the ContentStatusFlags defined above. It is used
    // by some tests for checking and for certain copying. Use the per-status
    // functions for normal usage.
    void set_content_status(int content_status) {
      content_status_ = content_status;
    }
    int content_status() const {
      return content_status_;
    }

   private:
    // See the accessors above for descriptions.
    content::SecurityStyle security_style_;
    int cert_id_;
    net::CertStatus cert_status_;
    int security_bits_;
    int connection_status_;
    int content_status_;

    // Copy and assignment is explicitly allowed for this class.
  };

  // Favicon -------------------------------------------------------------------

  // Collects the favicon related information for a NavigationEntry.
  class FaviconStatus {
   public:
    FaviconStatus();

    // Indicates whether we've gotten an official favicon for the page, or are
    // just using the default favicon.
    void set_is_valid(bool is_valid) {
      valid_ = is_valid;
    }
    bool is_valid() const {
      return valid_;
    }

    // The URL of the favicon which was used to load it off the web.
    void set_url(const GURL& favicon_url) {
      url_ = favicon_url;
    }
    const GURL& url() const {
      return url_;
    }

    // The favicon bitmap for the page. If the favicon has not been explicitly
    // set or it empty, it will return the default favicon. Note that this is
    // loaded asynchronously, so even if the favicon URL is valid we may return
    // the default favicon if we haven't gotten the data yet.
    void set_bitmap(const SkBitmap& bitmap) {
      bitmap_ = bitmap;
    }
    const SkBitmap& bitmap() const {
      return bitmap_;
    }

   private:
    // See the accessors above for descriptions.
    bool valid_;
    GURL url_;
    SkBitmap bitmap_;

    // Copy and assignment is explicitly allowed for this class.
  };

  // ---------------------------------------------------------------------------

  NavigationEntry();
  NavigationEntry(SiteInstance* instance,
                  int page_id,
                  const GURL& url,
                  const GURL& referrer,
                  const string16& title,
                  content::PageTransition transition_type,
                  bool is_renderer_initiated);
  ~NavigationEntry();

  // Page-related stuff --------------------------------------------------------

  // A unique ID is preserved across commits and redirects, which means that
  // sometimes a NavigationEntry's unique ID needs to be set (e.g. when
  // creating a committed entry to correspond to a to-be-deleted pending entry,
  // the pending entry's ID must be copied).
  void set_unique_id(int unique_id) {
    unique_id_ = unique_id;
  }
  int unique_id() const {
    return unique_id_;
  }

  // The SiteInstance tells us how to share sub-processes when the tab type is
  // TAB_CONTENTS_WEB. This will be NULL otherwise. This is a reference counted
  // pointer to a shared site instance.
  //
  // Note that the SiteInstance should usually not be changed after it is set,
  // but this may happen if the NavigationEntry was cloned and needs to use a
  // different SiteInstance.
  void set_site_instance(SiteInstance* site_instance);
  SiteInstance* site_instance() const {
    return site_instance_;
  }

  // The page type tells us if this entry is for an interstitial or error page.
  // See the PageType enum above.
  void set_page_type(content::PageType page_type) {
    page_type_ = page_type;
  }
  content::PageType page_type() const {
    return page_type_;
  }

  // The actual URL of the page. For some about pages, this may be a scary
  // data: URL or something like that. Use virtual_url() below for showing to
  // the user.
  void set_url(const GURL& url) {
    url_ = url;
    cached_display_title_.clear();
  }
  const GURL& url() const {
    return url_;
  }

  // The referring URL. Can be empty.
  void set_referrer(const GURL& referrer) {
    referrer_ = referrer;
  }
  const GURL& referrer() const {
    return referrer_;
  }

  // The virtual URL, when nonempty, will override the actual URL of the page
  // when we display it to the user. This allows us to have nice and friendly
  // URLs that the user sees for things like about: URLs, but actually feed
  // the renderer a data URL that results in the content loading.
  //
  // virtual_url() will return the URL to display to the user in all cases, so
  // if there is no overridden display URL, it will return the actual one.
  void set_virtual_url(const GURL& url) {
    virtual_url_ = (url == url_) ? GURL() : url;
    cached_display_title_.clear();
  }
  bool has_virtual_url() const {
    return !virtual_url_.is_empty();
  }
  const GURL& virtual_url() const {
    return virtual_url_.is_empty() ? url_ : virtual_url_;
  }

  bool update_virtual_url_with_url() const {
    return update_virtual_url_with_url_;
  }
  void set_update_virtual_url_with_url(bool update) {
    update_virtual_url_with_url_ = update;
  }

  // The title as set by the page. This will be empty if there is no title set.
  // The caller is responsible for detecting when there is no title and
  // displaying the appropriate "Untitled" label if this is being displayed to
  // the user.
  void set_title(const string16& title) {
    title_ = title;
    cached_display_title_.clear();
  }
  const string16& title() const {
    return title_;
  }

  // The favicon data and tracking information. See FaviconStatus above.
  const FaviconStatus& favicon() const {
    return favicon_;
  }
  FaviconStatus& favicon() {
    return favicon_;
  }

  // Content state is an opaque blob created by WebKit that represents the
  // state of the page. This includes form entries and scroll position for each
  // frame. We store it so that we can supply it back to WebKit to restore form
  // state properly when the user goes back and forward.
  //
  // WARNING: This state is saved to the file and used to restore previous
  // states. If the format is modified in the future, we should still be able to
  // deal with older versions.
  void set_content_state(const std::string& state) {
    content_state_ = state;
  }
  const std::string& content_state() const {
    return content_state_;
  }

  // Describes the current page that the tab represents. For web pages
  // (TAB_CONTENTS_WEB) this is the ID that the renderer generated for the page
  // and is how we can tell new versus renavigations.
  void set_page_id(int page_id) {
    page_id_ = page_id;
  }
  int32 page_id() const {
    return page_id_;
  }

  // All the SSL flags and state. See SSLStatus above.
  const SSLStatus& ssl() const {
    return ssl_;
  }
  SSLStatus& ssl() {
    return ssl_;
  }

  // Extra headers (separated by \n) to send during the request.
  void set_extra_headers(const std::string& extra_headers) {
    extra_headers_ = extra_headers;
  }
  const std::string& extra_headers() const {
    return extra_headers_;
  }

  // Page-related helpers ------------------------------------------------------

  // Returns the title to be displayed on the tab. This could be the title of
  // the page if it is available or the URL. |languages| is the list of
  // accpeted languages (e.g., prefs::kAcceptLanguages) or empty if proper
  // URL formatting isn't needed (e.g., unit tests).
  const string16& GetTitleForDisplay(const std::string& languages) const;

  // Returns true if the current tab is in view source mode. This will be false
  // if there is no navigation.
  bool IsViewSourceMode() const;

  // Tracking stuff ------------------------------------------------------------

  // The transition type indicates what the user did to move to this page from
  // the previous page.
  void set_transition_type(content::PageTransition transition_type) {
    transition_type_ = transition_type;
  }
  content::PageTransition transition_type() const {
    return transition_type_;
  }

  // Whether this (pending) navigation is renderer-initiated.  Resets to false
  // for all types of navigations after commit.
  void set_is_renderer_initiated(bool is_renderer_initiated) {
    is_renderer_initiated_ = is_renderer_initiated;
  }
  bool is_renderer_initiated() const {
    return is_renderer_initiated_;
  }

  // The user typed URL was the URL that the user initiated the navigation
  // with, regardless of any redirects. This is used to generate keywords, for
  // example, based on "what the user thinks the site is called" rather than
  // what it's actually called. For example, if the user types "foo.com", that
  // may redirect somewhere arbitrary like "bar.com/foo", and we want to use
  // the name that the user things of the site as having.
  //
  // This URL will be is_empty() if the URL was navigated to some other way.
  // Callers should fall back on using the regular or display URL in this case.
  void set_user_typed_url(const GURL& user_typed_url) {
    user_typed_url_ = user_typed_url;
  }
  const GURL& user_typed_url() const {
    return user_typed_url_;
  }

  // Post data is form data that was posted to get to this page. The data will
  // have to be reposted to reload the page properly. This flag indicates
  // whether the page had post data.
  //
  // The actual post data is stored in the content_state and is extracted by
  // WebKit to actually make the request.
  void set_has_post_data(bool has_post_data) {
    has_post_data_ = has_post_data;
  }
  bool has_post_data() const {
    return has_post_data_;
  }

  // Enumerations of the possible restore types.
  enum RestoreType {
    // The entry has been restored is from the last session.
    RESTORE_LAST_SESSION,

    // The entry has been restored from the current session. This is used when
    // the user issues 'reopen closed tab'.
    RESTORE_CURRENT_SESSION,

    // The entry was not restored.
    RESTORE_NONE
  };

  // The RestoreType for this entry. This is set if the entry was retored. This
  // is set to RESTORE_NONE once the entry is loaded.
  void set_restore_type(RestoreType type) {
    restore_type_ = type;
  }
  RestoreType restore_type() const {
    return restore_type_;
  }

  void set_transferred_global_request_id(
      const GlobalRequestID& transferred_global_request_id) {
    transferred_global_request_id_ = transferred_global_request_id;
  }

  GlobalRequestID transferred_global_request_id() const {
    return transferred_global_request_id_;
  }

 private:
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
  // Session/Tab restore save portions of this class so that it can be recreated
  // later. If you add a new field that needs to be persisted you'll have to
  // update SessionService/TabRestoreService appropriately.
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

  // See the accessors above for descriptions.
  int unique_id_;
  scoped_refptr<SiteInstance> site_instance_;
  content::PageType page_type_;
  GURL url_;
  GURL referrer_;
  GURL virtual_url_;
  bool update_virtual_url_with_url_;
  string16 title_;
  FaviconStatus favicon_;
  std::string content_state_;
  int32 page_id_;
  SSLStatus ssl_;
  content::PageTransition transition_type_;
  GURL user_typed_url_;
  bool has_post_data_;
  RestoreType restore_type_;

  // This member is not persisted with sesssion restore.
  std::string extra_headers_;

  // Whether the entry, while loading, was created for a renderer-initiated
  // navigation.  This dictates whether the URL should be displayed before the
  // navigation commits.  It is cleared on commit and not persisted.
  bool is_renderer_initiated_;

  // This is a cached version of the result of GetTitleForDisplay. It prevents
  // us from having to do URL formatting on the URL every time the title is
  // displayed. When the URL, virtual URL, or title is set, this should be
  // cleared to force a refresh.
  mutable string16 cached_display_title_;

  // In case a navigation is transferred to a new RVH but the request has
  // been generated in the renderer already, this identifies the old request so
  // that it can be resumed. The old request is stored until the
  // ResourceDispatcher receives the navigation from the renderer which
  // carries this |transferred_global_request_id_| annotation. Once the request
  // is transferred to the new process, this is cleared and the request
  // continues as normal.
  GlobalRequestID transferred_global_request_id_;

  // Copy and assignment is explicitly allowed for this class.
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_ENTRY_H_
