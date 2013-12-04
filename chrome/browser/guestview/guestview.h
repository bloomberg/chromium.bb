// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUESTVIEW_GUESTVIEW_H_
#define CHROME_BROWSER_GUESTVIEW_GUESTVIEW_H_

#include <queue>

#include "base/values.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/web_contents.h"

class AdViewGuest;
class WebViewGuest;
struct RendererContentSettingRules;

// A GuestView is the base class browser-side API implementation for a <*view>
// tag. GuestView maintains an association between a guest WebContents and an
// embedder WebContents. It receives events issued from the guest and relays
// them to the embedder.
class GuestView : public content::BrowserPluginGuestDelegate {
 public:
  enum Type {
    WEBVIEW,
    ADVIEW,
    UNKNOWN
  };

  class Event {
   public:
     Event(const std::string& name, scoped_ptr<DictionaryValue> args);
     ~Event();

    const std::string& name() const { return name_; }

    scoped_ptr<DictionaryValue> GetArguments();

   private:
    const std::string name_;
    scoped_ptr<DictionaryValue> args_;
  };

  static Type GetViewTypeFromString(const std::string& api_type);

  static GuestView* Create(content::WebContents* guest_web_contents,
                           const std::string& extension_id,
                           Type view_type);

  static GuestView* FromWebContents(content::WebContents* web_contents);

  static GuestView* From(int embedder_process_id, int instance_id);

  // For GuestViews, we create special guest processes, which host the
  // tag content separately from the main application that embeds the tag.
  // A GuestView can specify both the partition name and whether the storage
  // for that partition should be persisted. Each tag gets a SiteInstance with
  // a specially formatted URL, based on the application it is hosted by and
  // the partition requested by it. The format for that URL is:
  // chrome-guest://partition_domain/persist?partition_name
  static bool GetGuestPartitionConfigForSite(const GURL& site,
                                             std::string* partition_domain,
                                             std::string* partition_name,
                                             bool* in_memory);

  // By default, JavaScript and images are enabled in guest content.
  static void GetDefaultContentSettingRules(
      RendererContentSettingRules* rules, bool incognito);

  virtual void Attach(content::WebContents* embedder_web_contents,
                      const base::DictionaryValue& args);

  content::WebContents* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  // Returns the guest WebContents.
  content::WebContents* guest_web_contents() const {
    return guest_web_contents_;
  }

  virtual Type GetViewType() const;

  // Returns a WebViewGuest if this GuestView belongs to a <webview>.
  virtual WebViewGuest* AsWebView() = 0;

  // Returns an AdViewGuest if the GuestView belongs to an <adview>.
  virtual AdViewGuest* AsAdView() = 0;

  // Returns whether this guest has an associated embedder.
  bool attached() const { return !!embedder_web_contents_; }

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
  GuestView(content::WebContents* guest_web_contents,
            const std::string& extension_id);
  virtual ~GuestView();

  // Dispatches an event |event_name| to the embedder with the |event| fields.
  void DispatchEvent(Event* event);

 private:
  void SendQueuedEvents();

  content::WebContents* const guest_web_contents_;
  content::WebContents* embedder_web_contents_;
  const std::string extension_id_;
  int embedder_render_process_id_;
  content::BrowserContext* const browser_context_;
  // |guest_instance_id_| is a profile-wide unique identifier for a guest
  // WebContents.
  const int guest_instance_id_;
  // |view_instance_id_| is an identifier that's unique within a particular
  // embedder RenderViewHost for a particular <*view> instance.
  int view_instance_id_;

  // This is a queue of Events that are destined to be sent to the embedder once
  // the guest is attached to a particular embedder.
  std::queue<Event*> pending_events_;

  DISALLOW_COPY_AND_ASSIGN(GuestView);
};

#endif  // CHROME_BROWSER_GUESTVIEW_GUESTVIEW_H_
