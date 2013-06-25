// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ADVIEW_ADVIEW_GUEST_H_
#define CHROME_BROWSER_ADVIEW_ADVIEW_GUEST_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"

// An AdViewGuest is a WebContentsObserver on the guest WebContents of a
// <adview> tag. It provides the browser-side implementation of the <adview>
// API and manages the lifetime of <adview> extension events. AdViewGuest is
// created on attachment. When a guest WebContents is associated with
// a particular embedder WebContents, we call this "attachment".
// TODO(fsamuel): There might be an opportunity here to refactor and reuse code
// between AdViewGuest and WebViewGuest.
class AdViewGuest : public content::WebContentsObserver {
 public:
  AdViewGuest(content::WebContents* guest_web_contents,
              content::WebContents* embedder_web_contents,
              const std::string& extension_id,
              int adview_instance_id,
              const base::DictionaryValue& args);

  static AdViewGuest* From(int embedder_process_id, int instance_id);

  content::WebContents* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  content::WebContents* web_contents() const {
    return WebContentsObserver::web_contents();
  }

  int view_instance_id() const { return view_instance_id_; }

 private:
  virtual ~AdViewGuest();

  void DispatchEvent(const std::string& event_name,
                     scoped_ptr<DictionaryValue> event);

  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  content::WebContents* embedder_web_contents_;
  const std::string extension_id_;
  const int embedder_render_process_id_;
  // Profile and instance ID are cached here because |web_contents()| is
  // null on destruction.
  void* profile_;
  // |guest_instance_id_| is a profile-wide unique identifier for a guest
  // WebContents.
  const int guest_instance_id_;
  // |view_instance_id_| is an identifier that's unique within a particular
  // embedder RenderView for a particular <adview> instance.
  const int view_instance_id_;

  DISALLOW_COPY_AND_ASSIGN(AdViewGuest);
};

#endif  // CHROME_BROWSER_ADVIEW_ADVIEW_GUEST_H_
