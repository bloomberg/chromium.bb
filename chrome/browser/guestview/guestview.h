// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUESTVIEW_GUESTVIEW_H_
#define CHROME_BROWSER_GUESTVIEW_GUESTVIEW_H_

#include "base/values.h"
#include "content/public/browser/web_contents.h"

class AdViewGuest;
class WebViewGuest;

// A GuestView is the base class browser-side API implementation for a <*view>
// tag. GuestView maintains an association between a guest WebContents and an
// embedder WebContents. It receives events issued from the guest and relays
// them to the embedder.
class GuestView {
 public:
  GuestView(content::WebContents* guest_web_contents,
            content::WebContents* embedder_web_contents,
            const std::string& extension_id,
            int view_instance_id,
            const base::DictionaryValue& args);

  static GuestView* From(int embedder_process_id, int instance_id);

  content::WebContents* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  // Returns the guest WebContents.
  virtual content::WebContents* GetWebContents() const = 0;

  // Returns a WebViewGuest if this GuestView belongs to a <webview>.
  virtual WebViewGuest* AsWebView() = 0;

  // Returns an AdViewGuest if the GuestView belongs to an <adview>.
  virtual AdViewGuest* AsAdView() = 0;

  // Returns the instance ID of the <*view> element.
  int view_instance_id() const { return view_instance_id_; }

  // Returns the instance ID of the guest WebContents.
  int guest_instance_id() const { return guest_instance_id_; }

  // Returns the extension ID of the embedder.
  const std::string& extension_id() const { return extension_id_; }

  // Returns the user browser context of the embedder.
  content::BrowserContext* browser_context() const { return browser_context_; }

  // Returns the embedder's process ID.
  int embedder_render_process_id() const { return embedder_render_process_id_; }

 protected:
  virtual ~GuestView();

  // Dispatches an event |event_name| to the embedder with the |event| fields.
  void DispatchEvent(const std::string& event_name,
                     scoped_ptr<DictionaryValue> event);

 private:
  content::WebContents* embedder_web_contents_;
  const std::string extension_id_;
  const int embedder_render_process_id_;
  // Profile and instance ID are cached here because |guest_web_contents| may be
  // null on destruction.
  content::BrowserContext* browser_context_;
  // |guest_instance_id_| is a profile-wide unique identifier for a guest
  // WebContents.
  const int guest_instance_id_;
  // |view_instance_id_| is an identifier that's unique within a particular
  // embedder RenderViewHost for a particular <*view> instance.
  const int view_instance_id_;

  DISALLOW_COPY_AND_ASSIGN(GuestView);
};

#endif  // CHROME_BROWSER_GUESTVIEW_GUESTVIEW_H_
