// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_DELEGATE_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_DELEGATE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/renderer/pepper_helper.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"
#include "ui/base/ime/text_input_type.h"

namespace base {
class FilePath;
}

namespace ppapi {
class PpapiPermissions;
struct URLResponseInfoData;
}

namespace WebKit {
class WebURLResponse;
struct WebCompositionUnderline;
struct WebCursorInfo;
}

namespace content {
class ContextProviderCommandBuffer;
class PluginModule;
class RenderViewImpl;
struct WebPluginInfo;

class PepperHelperImpl : public PepperHelper,
                         public base::SupportsWeakPtr<PepperHelperImpl>,
                         public RenderViewObserver {
 public:
  explicit PepperHelperImpl(RenderViewImpl* render_view);
  virtual ~PepperHelperImpl();

  RenderViewImpl* render_view() { return render_view_; }

  // Notifies that |instance| has changed the cursor.
  // This will update the cursor appearance if it is currently over the plugin
  // instance.
  void DidChangeCursor(PepperPluginInstanceImpl* instance,
                       const WebKit::WebCursorInfo& cursor);

  // Notifies that |instance| has received a mouse event.
  void DidReceiveMouseEvent(PepperPluginInstanceImpl* instance);

  // Notification that the given plugin is focused or unfocused.
  void PluginFocusChanged(PepperPluginInstanceImpl* instance, bool focused);

  // Notification that the text input status of the given plugin is changed.
  void PluginTextInputTypeChanged(PepperPluginInstanceImpl* instance);

  // Notification that the caret position in the given plugin is changed.
  void PluginCaretPositionChanged(PepperPluginInstanceImpl* instance);

  // Notification that the plugin requested to cancel the current composition.
  void PluginRequestedCancelComposition(PepperPluginInstanceImpl* instance);

  // Notification that the text selection in the given plugin is changed.
  void PluginSelectionChanged(PepperPluginInstanceImpl* instance);

  // Indicates that the given instance has been created.
  void InstanceCreated(PepperPluginInstanceImpl* instance);

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  void InstanceDeleted(PepperPluginInstanceImpl* instance);

  // Sets up the renderer host and out-of-process proxy for an external plugin
  // module. Returns the renderer host, or NULL if it couldn't be created.
  RendererPpapiHost* CreateExternalPluginModule(
      scoped_refptr<PluginModule> module,
      const base::FilePath& path,
      ::ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id);

 private:
  // PepperHelper implementation.
  virtual WebKit::WebPlugin* CreatePepperWebPlugin(
    const WebPluginInfo& webplugin_info,
    const WebKit::WebPluginParams& params) OVERRIDE;
  virtual void ViewWillInitiatePaint() OVERRIDE;
  virtual void ViewInitiatedPaint() OVERRIDE;
  virtual void ViewFlushedPaint() OVERRIDE;
  virtual PepperPluginInstanceImpl* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip,
      float* scale_factor) OVERRIDE;
  virtual void OnSetFocus(bool has_focus) OVERRIDE;
  virtual void PageVisibilityChanged(bool is_visible) OVERRIDE;
  virtual bool IsPluginFocused() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() const OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual bool IsPluginAcceptingCompositionEvents() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual void GetSurroundingText(string16* text,
                                  ui::Range* range) const OVERRIDE;
  virtual void OnImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) OVERRIDE;
  virtual void OnImeConfirmComposition(const string16& text) OVERRIDE;
  virtual void WillHandleMouseEvent() OVERRIDE;

  // RenderViewObserver implementation.
  virtual void OnDestruct() OVERRIDE;

  // Attempts to create a PPAPI plugin for the given filepath. On success, it
  // will return the newly-created module.
  //
  // There are two reasons for failure. The first is that the plugin isn't
  // a PPAPI plugin. In this case, |*pepper_plugin_was_registered| will be set
  // to false and the caller may want to fall back on creating an NPAPI plugin.
  // the second is that the plugin failed to initialize. In this case,
  // |*pepper_plugin_was_registered| will be set to true and the caller should
  // not fall back on any other plugin types.
  scoped_refptr<PluginModule> CreatePepperPluginModule(
      const WebPluginInfo& webplugin_info,
      bool* pepper_plugin_was_registered);

  // Create a new HostDispatcher for proxying, hook it to the PluginModule,
  // and perform other common initialization.
  RendererPpapiHost* CreateOutOfProcessModule(
      PluginModule* module,
      const base::FilePath& path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id,
      bool is_external);

  // Pointer to the RenderView that owns us.
  RenderViewImpl* render_view_;

  std::set<PepperPluginInstanceImpl*> active_instances_;

  // Whether or not the focus is on a PPAPI plugin
  PepperPluginInstanceImpl* focused_plugin_;

  // Current text input composition text. Empty if no composition is in
  // progress.
  string16 composition_text_;

  // The plugin instance that received the last mouse event. It is set to NULL
  // if the last mouse event went to elements other than Pepper plugins.
  // |last_mouse_event_target_| is not owned by this class. We can know about
  // when it is destroyed via InstanceDeleted().
  PepperPluginInstanceImpl* last_mouse_event_target_;

  scoped_refptr<ContextProviderCommandBuffer> offscreen_context3d_;

  DISALLOW_COPY_AND_ASSIGN(PepperHelperImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_DELEGATE_H_
