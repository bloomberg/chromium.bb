// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginGuest represents the browser side of browser <--> renderer
// communication. A BrowserPlugin (a WebPlugin) is on the renderer side of
// browser <--> guest renderer communication. The 'guest' renderer is a
// <browser> tag.
//
// BrowserPluginGuest lives on the UI thread of the browser process. It has a
// helper, BrowserPluginGuestHelper, which is a RenderViewHostObserver. The
// helper object receives messages (ViewHostMsg_*) directed at the browser
// plugin and redirects them to this class. Any messages the embedder might be
// interested in knowing or modifying about the guest should be listened for
// here.
//
// Since BrowserPlugin is a WebPlugin, we need to provide overridden behaviors
// for messages like handleInputEvent, updateGeometry. Such messages get
// routed into BrowserPluginGuest via its embedder (BrowserPluginEmbedder).
// These are BrowserPluginHost_* messages sent from the BrowserPlugin.
//
// BrowserPluginGuest knows about its embedder process. Communication to
// renderer happens through the embedder process.
//
// A BrowserPluginGuest is also associated directly with the WebContents related
// to the BrowserPlugin. BrowserPluginGuest is a WebContentsDelegate and
// WebContentsObserver for the WebContents.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/time.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/webcursor.h"

class TransportDIB;
struct ViewHostMsg_UpdateRect_Params;

namespace WebKit {
class WebInputEvent;
}

namespace content {

class BrowserPluginHostFactory;
class BrowserPluginEmbedder;
class RenderProcessHost;

// A browser plugin guest provides functionality for WebContents to operate in
// the guest role and implements guest specific overrides for ViewHostMsg_*
// messages.
//
// BrowserPluginEmbedder is responsible for creating and destroying a guest.
class CONTENT_EXPORT BrowserPluginGuest : public WebContentsDelegate,
                                          public WebContentsObserver {
 public:
  virtual ~BrowserPluginGuest();

  static BrowserPluginGuest* Create(int instance_id,
                                    WebContentsImpl* web_contents,
                                    content::RenderViewHost* render_view_host);

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(BrowserPluginHostFactory* factory) {
    content::BrowserPluginGuest::factory_ = factory;
  }

  void set_guest_hang_timeout_for_testing(const base::TimeDelta& timeout) {
    guest_hang_timeout_ = timeout;
  }

  void set_embedder_render_process_host(
      RenderProcessHost* render_process_host) {
    embedder_render_process_host_ = render_process_host;
  }

  // WebContentsObserver implementation.
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  // WebContentsDelegate implementation.
  virtual void RendererUnresponsive(WebContents* source) OVERRIDE;

  void SetDamageBuffer(TransportDIB* damage_buffer,
#if defined(OS_WIN)
                       int damage_buffer_size,
#endif
                       const gfx::Size& damage_view_size,
                       float scale_factor);

  void UpdateRect(RenderViewHost* render_view_host,
                  const ViewHostMsg_UpdateRect_Params& params);
  void UpdateRectACK(int message_id, const gfx::Size& size);
  // Handles input event routed through the embedder (which is initiated in the
  // browser plugin (renderer side of the embedder)).
  void HandleInputEvent(RenderViewHost* render_view_host,
                        const gfx::Rect& guest_rect,
                        const WebKit::WebInputEvent& event,
                        IPC::Message* reply_message);
  // Overrides default ShowWidget message so we show them on the correct
  // coordinates.
  void ShowWidget(RenderViewHost* render_view_host,
                  int route_id,
                  const gfx::Rect& initial_pos);
  void SetCursor(const WebCursor& cursor);
  // Handles input event acks so they are sent to browser plugin host (via
  // embedder) instead of default view/widget host.
  void HandleInputEventAck(RenderViewHost* render_view_host, bool handled);

  // Exposes the protected web_contents() from WebContentsObserver.
  WebContents* GetWebContents();

  // Overridden in tests.
  virtual bool ViewTakeFocus(bool reverse);
  // Overridden in tests.
  virtual void SetFocus(bool focused);
  // Reload the guest.
  virtual void Reload();
  // Stop loading the guest.
  virtual void Stop();

 private:
  friend class TestBrowserPluginGuest;

  BrowserPluginGuest(int instance_id,
                     WebContentsImpl* web_contents,
                     RenderViewHost* render_view_host);

  RenderProcessHost* embedder_render_process_host() {
    return embedder_render_process_host_;
  }
  // Returns the identifier that uniquely identifies a browser plugin guest
  // within an embedder.
  int instance_id() const { return instance_id_; }
  TransportDIB* damage_buffer() const { return damage_buffer_.get(); }
  const gfx::Size& damage_view_size() const { return damage_view_size_; }
  float damage_buffer_scale_factor() const {
    return damage_buffer_scale_factor_;
  }

  // Helper to send messages to embedder. Overridden in test implementation
  // since we want to intercept certain messages for testing.
  virtual void SendMessageToEmbedder(IPC::Message* msg);

  // Static factory instance (always NULL for non-test).
  static content::BrowserPluginHostFactory* factory_;

  RenderProcessHost* embedder_render_process_host_;
  // An identifier that uniquely identifies a browser plugin guest within an
  // embedder.
  int instance_id_;
  scoped_ptr<TransportDIB> damage_buffer_;
#if defined(OS_WIN)
  size_t damage_buffer_size_;
#endif
  gfx::Size damage_view_size_;
  float damage_buffer_scale_factor_;
  scoped_ptr<IPC::Message> pending_input_event_reply_;
  gfx::Rect guest_rect_;
  WebCursor cursor_;
  IDMap<RenderViewHost> pending_updates_;
  int pending_update_counter_;
  base::TimeDelta guest_hang_timeout_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
