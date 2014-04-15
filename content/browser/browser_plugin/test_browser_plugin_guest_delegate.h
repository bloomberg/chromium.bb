// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "content/public/browser/browser_plugin_guest_delegate.h"

namespace content {

class TestBrowserPluginGuestDelegate : public BrowserPluginGuestDelegate {
 public:
  TestBrowserPluginGuestDelegate();
  virtual ~TestBrowserPluginGuestDelegate();

  void ResetStates();

  bool load_aborted() const { return load_aborted_; }
  const GURL& load_aborted_url() const { return load_aborted_url_; }

 private:
  // Overridden from BrowserPluginGuestDelegate:
  virtual void AddMessageToConsole(int32 level,
                                   const base::string16& message,
                                   int32 line_no,
                                   const base::string16& source_id) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void GuestProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual bool HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void LoadAbort(bool is_top_level,
                         const GURL& url,
                         const std::string& error_type) OVERRIDE;
  virtual void RendererResponsive() OVERRIDE;
  virtual void RendererUnresponsive() OVERRIDE;
  virtual void RequestPermission(
      BrowserPluginPermissionType permission_type,
      const base::DictionaryValue& request_info,
      const PermissionResponseCallback& callback,
      bool allowed_by_default) OVERRIDE;
  virtual void SizeChanged(const gfx::Size& old_size,
                           const gfx::Size& new_size) OVERRIDE;

  bool load_aborted_;
  GURL load_aborted_url_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginGuestDelegate);
};

}  // namespace content
#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_DELEGATE_H_
