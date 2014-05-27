// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_BASE_H_
#define CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_BASE_H_

#include <queue>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

struct RendererContentSettingRules;

// A GuestViewBase is the base class browser-side API implementation for a
// <*view> tag. GuestViewBase maintains an association between a guest
// WebContents and an embedder WebContents. It receives events issued from
// the guest and relays them to the embedder.
class GuestViewBase : public content::BrowserPluginGuestDelegate,
                      public content::WebContentsDelegate {
 public:
  class Event {
   public:
    Event(const std::string& name, scoped_ptr<base::DictionaryValue> args);
    ~Event();

    const std::string& name() const { return name_; }

    scoped_ptr<base::DictionaryValue> GetArguments();

   private:
    const std::string name_;
    scoped_ptr<base::DictionaryValue> args_;
  };

  // Returns a *ViewGuest if this GuestView is of the given view type.
  template <typename T>
  T* As() {
    if (IsViewType(T::Type))
      return static_cast<T*>(this);

    return NULL;
  }

  static GuestViewBase* Create(int guest_instance_id,
                               content::WebContents* guest_web_contents,
                               const std::string& embedder_extension_id,
                               const std::string& view_type);

  static GuestViewBase* FromWebContents(content::WebContents* web_contents);

  static GuestViewBase* From(int embedder_process_id, int instance_id);

  static bool IsGuest(content::WebContents* web_contents);

  // For GuestViewBases, we create special guest processes, which host the
  // tag content separately from the main application that embeds the tag.
  // A GuestViewBase can specify both the partition name and whether the storage
  // for that partition should be persisted. Each tag gets a SiteInstance with
  // a specially formatted URL, based on the application it is hosted by and
  // the partition requested by it. The format for that URL is:
  // chrome-guest://partition_domain/persist?partition_name
  static bool GetGuestPartitionConfigForSite(const GURL& site,
                                             std::string* partition_domain,
                                             std::string* partition_name,
                                             bool* in_memory);

  // By default, JavaScript and images are enabled in guest content.
  static void GetDefaultContentSettingRules(RendererContentSettingRules* rules,
                                            bool incognito);

  virtual const char* GetViewType() const = 0;

  bool IsViewType(const char* const view_type) const {
    return !strcmp(GetViewType(), view_type);
  }

  base::WeakPtr<GuestViewBase> AsWeakPtr();

  virtual void Attach(content::WebContents* embedder_web_contents,
                      const base::DictionaryValue& args);

  content::WebContents* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  // Returns the guest WebContents.
  content::WebContents* guest_web_contents() const {
    return guest_web_contents_;
  }

  // Returns the extra parameters associated with this GuestView passed
  // in from JavaScript.
  base::DictionaryValue* extra_params() const {
    return extra_params_.get();
  }

  // Returns whether this guest has an associated embedder.
  bool attached() const { return !!embedder_web_contents_; }

  // Returns the instance ID of the <*view> element.
  int view_instance_id() const { return view_instance_id_; }

  // Returns the instance ID of the guest WebContents.
  int guest_instance_id() const { return guest_instance_id_; }

  // Returns the extension ID of the embedder.
  const std::string& embedder_extension_id() const {
    return embedder_extension_id_;
  }

  // Returns whether this GuestView is embedded in an extension/app.
  bool in_extension() const { return !embedder_extension_id_.empty(); }

  // Returns the user browser context of the embedder.
  content::BrowserContext* browser_context() const { return browser_context_; }

  // Returns the embedder's process ID.
  int embedder_render_process_id() const { return embedder_render_process_id_; }

  GuestViewBase* GetOpener() const {
    return opener_.get();
  }

  void SetOpener(GuestViewBase* opener);

  // WebContentsDelegate implementation.
  virtual bool ShouldFocusPageAfterCrash() OVERRIDE;
  virtual bool PreHandleGestureEvent(
      content::WebContents* source,
      const blink::WebGestureEvent& event) OVERRIDE;

  // BrowserPluginGuestDelegate implementation.
  virtual void Destroy() OVERRIDE;
  virtual void RegisterDestructionCallback(
      const DestructionCallback& callback) OVERRIDE;
 protected:
  GuestViewBase(int guest_instance_id,
                content::WebContents* guest_web_contents,
                const std::string& embedder_extension_id);
  virtual ~GuestViewBase();

  // Dispatches an event |event_name| to the embedder with the |event| fields.
  void DispatchEvent(Event* event);

 private:
  void SendQueuedEvents();

  content::WebContents* const guest_web_contents_;
  content::WebContents* embedder_web_contents_;
  const std::string embedder_extension_id_;
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
  std::deque<linked_ptr<Event> > pending_events_;

  // The opener guest view.
  base::WeakPtr<GuestViewBase> opener_;

  DestructionCallback destruction_callback_;

  // The extra parameters associated with this GuestView passed
  // in from JavaScript. This will typically be the view instance ID,
  // the API to use, and view-specific parameters. These parameters
  // are passed along to new guests that are created from this guest.
  scoped_ptr<base::DictionaryValue> extra_params_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<GuestViewBase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewBase);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_BASE_H_
