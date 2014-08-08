// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_BASE_H_
#define CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_BASE_H_

#include <queue>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

struct RendererContentSettingRules;

// A GuestViewBase is the base class browser-side API implementation for a
// <*view> tag. GuestViewBase maintains an association between a guest
// WebContents and an embedder WebContents. It receives events issued from
// the guest and relays them to the embedder. GuestViewBase tracks the lifetime
// of its embedder render process until it is attached to a particular embedder
// WebContents. At that point, its lifetime is restricted in scope to the
// lifetime of its embedder WebContents.
class GuestViewBase : public content::BrowserPluginGuestDelegate,
                      public content::RenderProcessHostObserver,
                      public content::WebContentsDelegate,
                      public content::WebContentsObserver {
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

  typedef base::Callback<GuestViewBase*(
      content::BrowserContext*, int)> GuestCreationCallback;
  static void RegisterGuestViewType(const std::string& view_type,
                                    const GuestCreationCallback& callback);

  static GuestViewBase* Create(content::BrowserContext* browser_context,
                               int guest_instance_id,
                               const std::string& view_type);

  static GuestViewBase* FromWebContents(content::WebContents* web_contents);

  static GuestViewBase* From(int embedder_process_id, int instance_id);

  static bool IsGuest(content::WebContents* web_contents);

  // By default, JavaScript and images are enabled in guest content.
  static void GetDefaultContentSettingRules(RendererContentSettingRules* rules,
                                            bool incognito);

  virtual const char* GetViewType() const = 0;

  // This method is called after the guest has been attached to an embedder and
  // suspended resource loads have been resumed.
  //
  // This method can be overriden by subclasses. This gives the derived class
  // an opportunity to perform setup actions after attachment.
  virtual void DidAttachToEmbedder() {}

  // This method is called after this GuestViewBase has been initiated.
  //
  // This gives the derived class an opportunity to perform additional
  // initialization.
  virtual void DidInitialize() {}

  // This method is called when the initial set of frames within the page have
  // completed loading.
  virtual void DidStopLoading() {}

  // This method is called when the guest's embedder WebContents has been
  // destroyed and the guest will be destroyed shortly.
  //
  // This gives the derived class an opportunity to perform some cleanup prior
  // to destruction.
  virtual void EmbedderDestroyed() {}

  // This method is called when the guest WebContents has been destroyed. This
  // object will be destroyed after this call returns.
  //
  // This gives the derived class an opportunity to perform some cleanup.
  virtual void GuestDestroyed() {}

  // This method is invoked when the guest RenderView is ready, e.g. because we
  // recreated it after a crash.
  //
  // This gives the derived class an opportunity to perform some initialization
  // work.
  virtual void GuestReady() {}

  // This method is invoked when the contents auto-resized to give the container
  // an opportunity to match it if it wishes.
  //
  // This gives the derived class an opportunity to inform its container element
  // or perform other actions.
  virtual void GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                             const gfx::Size& new_size) {}

  // This method queries whether autosize is supported for this particular view.
  // By default, autosize is not supported. Derived classes can override this
  // behavior to support autosize.
  virtual bool IsAutoSizeSupported() const;

  // This method queries whether drag-and-drop is enabled for this particular
  // view. By default, drag-and-drop is disabled. Derived classes can override
  // this behavior to enable drag-and-drop.
  virtual bool IsDragAndDropEnabled() const;

  // This method is called immediately before suspended resource loads have been
  // resumed on attachment to an embedder.
  //
  // This method can be overriden by subclasses. This gives the derived class
  // an opportunity to perform setup actions before attachment.
  virtual void WillAttachToEmbedder() {}

  // This method is called when the guest WebContents is about to be destroyed.
  //
  // This gives the derived class an opportunity to perform some cleanup prior
  // to destruction.
  virtual void WillDestroy() {}

  // This method is to be implemented by the derived class. Access to guest
  // views are determined by the availability of the internal extension API
  // used to implement the guest view.
  //
  // This should be the name of the API as it appears in the _api_features.json
  // file.
  virtual const char* GetAPINamespace() = 0;

  // This method is to be implemented by the derived class. Given a set of
  // initialization parameters, a concrete subclass of GuestViewBase can
  // create a specialized WebContents that it returns back to GuestViewBase.
  typedef base::Callback<void(content::WebContents*)>
      WebContentsCreatedCallback;
  virtual void CreateWebContents(
      const std::string& embedder_extension_id,
      int embedder_render_process_id,
      const base::DictionaryValue& create_params,
      const WebContentsCreatedCallback& callback) = 0;

  // This creates a WebContents and initializes |this| GuestViewBase to use the
  // newly created WebContents.
  void Init(const std::string& embedder_extension_id,
            content::WebContents* embedder_web_contents,
            const base::DictionaryValue& create_params,
            const WebContentsCreatedCallback& callback);

  void InitWithWebContents(
      const std::string& embedder_extension_id,
      int embedder_render_process_id,
      content::WebContents* guest_web_contents);

  bool IsViewType(const char* const view_type) const {
    return !strcmp(GetViewType(), view_type);
  }

  // Toggles autosize mode for this GuestView.
  void SetAutoSize(bool enabled,
                   const gfx::Size& min_size,
                   const gfx::Size& max_size);

  base::WeakPtr<GuestViewBase> AsWeakPtr();

  bool initialized() const { return initialized_; }

  content::WebContents* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  // Returns the guest WebContents.
  content::WebContents* guest_web_contents() const {
    return web_contents();
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

  // RenderProcessHostObserver implementation
  virtual void RenderProcessExited(content::RenderProcessHost* host,
                                   base::ProcessHandle handle,
                                   base::TerminationStatus status,
                                   int exit_code) OVERRIDE;

  // BrowserPluginGuestDelegate implementation.
  virtual void Destroy() OVERRIDE FINAL;
  virtual void DidAttach() OVERRIDE FINAL;
  virtual void ElementSizeChanged(const gfx::Size& old_size,
                                  const gfx::Size& new_size) OVERRIDE FINAL;
  virtual int GetGuestInstanceID() const OVERRIDE;
  virtual void GuestSizeChanged(const gfx::Size& old_size,
                                const gfx::Size& new_size) OVERRIDE FINAL;
  virtual void RegisterDestructionCallback(
      const DestructionCallback& callback) OVERRIDE FINAL;
  virtual void WillAttach(
      content::WebContents* embedder_web_contents,
      const base::DictionaryValue& extra_params) OVERRIDE FINAL;

  // Dispatches an event |event_name| to the embedder with the |event| fields.
  void DispatchEventToEmbedder(Event* event);

 protected:
  GuestViewBase(content::BrowserContext* browser_context,
                int guest_instance_id);

  virtual ~GuestViewBase();

 private:
  class EmbedderWebContentsObserver;

  void SendQueuedEvents();

  void CompleteInit(const std::string& embedder_extension_id,
                    int embedder_render_process_id,
                    const WebContentsCreatedCallback& callback,
                    content::WebContents* guest_web_contents);

  static void RegisterGuestViewTypes();

  // WebContentsObserver implementation.
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE FINAL;
  virtual void RenderViewReady() OVERRIDE FINAL;
  virtual void WebContentsDestroyed() OVERRIDE FINAL;

  // WebContentsDelegate implementation.
  virtual bool ShouldFocusPageAfterCrash() OVERRIDE FINAL;
  virtual bool PreHandleGestureEvent(
      content::WebContents* source,
      const blink::WebGestureEvent& event) OVERRIDE FINAL;

  content::WebContents* embedder_web_contents_;
  std::string embedder_extension_id_;
  int embedder_render_process_id_;
  content::BrowserContext* browser_context_;
  // |guest_instance_id_| is a profile-wide unique identifier for a guest
  // WebContents.
  const int guest_instance_id_;
  // |view_instance_id_| is an identifier that's unique within a particular
  // embedder RenderViewHost for a particular <*view> instance.
  int view_instance_id_;

  bool initialized_;

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

  scoped_ptr<EmbedderWebContentsObserver> embedder_web_contents_observer_;

  // The size of the container element.
  gfx::Size element_size_;

  // The size of the guest content. Note: In autosize mode, the container
  // element may not match the size of the guest.
  gfx::Size guest_size_;

  // Indicates whether autosize mode is enabled or not.
  bool auto_size_enabled_;

  // The maximum size constraints of the container element in autosize mode.
  gfx::Size max_auto_size_;

  // The minimum size constraints of the container element in autosize mode.
  gfx::Size min_auto_size_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<GuestViewBase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewBase);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_BASE_H_
