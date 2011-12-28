// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_ENTRY_H_
#define CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_ENTRY_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"

class SiteInstance;

class CONTENT_EXPORT NavigationEntry
    : public NON_EXPORTED_BASE(content::NavigationEntry) {
 public:

  static NavigationEntry* FromNavigationEntry(content::NavigationEntry* entry);

  NavigationEntry();
  NavigationEntry(SiteInstance* instance,
                  int page_id,
                  const GURL& url,
                  const content::Referrer& referrer,
                  const string16& title,
                  content::PageTransition transition_type,
                  bool is_renderer_initiated);
  virtual ~NavigationEntry();

  // content::NavigationEntry implementation:
  virtual int GetUniqueID() const OVERRIDE;
  virtual content::PageType GetPageType() const OVERRIDE;
  virtual void SetURL(const GURL& url) OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual const content::Referrer& GetReferrer() const OVERRIDE;
  virtual void SetVirtualURL(const GURL& url) OVERRIDE;
  virtual const GURL& GetVirtualURL() const OVERRIDE;
  virtual void SetTitle(const string16& title) OVERRIDE;
  virtual const string16& GetTitle() const OVERRIDE;
  virtual void SetContentState(const std::string& state) OVERRIDE;
  virtual const std::string& GetContentState() const OVERRIDE;
  virtual void SetPageID(int page_id) OVERRIDE;
  virtual int32 GetPageID() const OVERRIDE;
  virtual const string16& GetTitleForDisplay(
      const std::string& languages) const OVERRIDE;
  virtual bool IsViewSourceMode() const OVERRIDE;
  virtual content::PageTransition GetTransitionType() const OVERRIDE;
  virtual const GURL& GetUserTypedURL() const OVERRIDE;
  virtual void SetHasPostData(bool has_post_data) OVERRIDE;
  virtual bool GetHasPostData() const OVERRIDE;
  virtual const content::FaviconStatus& GetFavicon() const OVERRIDE;
  virtual content::FaviconStatus& GetFavicon() OVERRIDE;
  virtual const content::SSLStatus& GetSSL() const OVERRIDE;
  virtual content::SSLStatus& GetSSL() OVERRIDE;

  void set_unique_id(int unique_id) {
    unique_id_ = unique_id;
  }

  void set_transition_type(content::PageTransition transition_type) {
    transition_type_ = transition_type;
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

  void set_page_type(content::PageType page_type) {
    page_type_ = page_type;
  }

  void set_referrer(const content::Referrer& referrer) {
    referrer_ = referrer;
  }

  bool has_virtual_url() const {
    return !virtual_url_.is_empty();
  }

  bool update_virtual_url_with_url() const {
    return update_virtual_url_with_url_;
  }
  void set_update_virtual_url_with_url(bool update) {
    update_virtual_url_with_url_ = update;
  }

  // Extra headers (separated by \n) to send during the request.
  void set_extra_headers(const std::string& extra_headers) {
    extra_headers_ = extra_headers;
  }
  const std::string& extra_headers() const {
    return extra_headers_;
  }

  // Whether this (pending) navigation is renderer-initiated.  Resets to false
  // for all types of navigations after commit.
  void set_is_renderer_initiated(bool is_renderer_initiated) {
    is_renderer_initiated_ = is_renderer_initiated;
  }
  bool is_renderer_initiated() const {
    return is_renderer_initiated_;
  }

  void set_user_typed_url(const GURL& user_typed_url) {
    user_typed_url_ = user_typed_url;
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
      const content::GlobalRequestID& transferred_global_request_id) {
    transferred_global_request_id_ = transferred_global_request_id;
  }

  content::GlobalRequestID transferred_global_request_id() const {
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
  content::Referrer referrer_;
  GURL virtual_url_;
  bool update_virtual_url_with_url_;
  string16 title_;
  content::FaviconStatus favicon_;
  std::string content_state_;
  int32 page_id_;
  content::SSLStatus ssl_;
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
  content::GlobalRequestID transferred_global_request_id_;

  // Copy and assignment is explicitly allowed for this class.
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_ENTRY_H_
