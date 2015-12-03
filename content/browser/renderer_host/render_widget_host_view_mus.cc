// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mus.h"

#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "content/browser/mojo/mojo_shell_client_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/render_widget_window_tree_client_factory.mojom.h"
#include "content/public/common/mojo_shell_connection.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace blink {
struct WebScreenInfo;
}

namespace content {

RenderWidgetHostViewMus::RenderWidgetHostViewMus(
    mus::Window* parent_window,
    RenderWidgetHostImpl* host,
    base::WeakPtr<RenderWidgetHostViewBase> platform_view)
    : host_(host), platform_view_(platform_view) {
  DCHECK(parent_window);
  mus::Window* window = parent_window->connection()->NewWindow();
  window->SetVisible(true);
  window->SetBounds(gfx::Rect(300, 300));
  parent_window->AddChild(window);
  window_.reset(new mus::ScopedWindowPtr(window));
  host_->SetView(this);

  // Connect to the renderer, pass it a WindowTreeClient interface request
  // and embed that client inside our mus window.
  std::string url = GetMojoApplicationInstanceURL(host_->GetProcess());
  mojom::RenderWidgetWindowTreeClientFactoryPtr factory;
  MojoShellConnection::Get()->GetApplication()->ConnectToService(url, &factory);

  mus::mojom::WindowTreeClientPtr window_tree_client;
  factory->CreateWindowTreeClientForRenderWidget(
      host_->GetRoutingID(), mojo::GetProxy(&window_tree_client));
  window_->window()->Embed(window_tree_client.Pass());
}

RenderWidgetHostViewMus::~RenderWidgetHostViewMus() {}

void RenderWidgetHostViewMus::Show() {
  // TODO(fsamuel): Update visibility in Mus.
  // There is some interstitial complexity that we'll need to figure out here.
  host_->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewMus::Hide() {
  host_->WasHidden();
}

bool RenderWidgetHostViewMus::IsShowing() {
  return !host_->is_hidden();
}

void RenderWidgetHostViewMus::SetSize(const gfx::Size& size) {
  size_ = size;
  gfx::Rect bounds = window_->window()->bounds();
  // TODO(fsamuel): figure out position.
  bounds.set_x(10);
  bounds.set_y(150);
  bounds.set_size(size);
  window_->window()->SetBounds(bounds);
  host_->WasResized();
}

void RenderWidgetHostViewMus::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

void RenderWidgetHostViewMus::Focus() {
  // TODO(fsamuel): Request focus for the associated Mus::Window
  // We need to be careful how we propagate focus as we navigate to and
  // from interstitials.
}

bool RenderWidgetHostViewMus::HasFocus() const {
  return true;
}

bool RenderWidgetHostViewMus::IsSurfaceAvailableForCopy() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect RenderWidgetHostViewMus::GetViewBounds() const {
  return gfx::Rect(size_);
}

gfx::Vector2dF RenderWidgetHostViewMus::GetLastScrollOffset() const {
  return gfx::Vector2dF();
}

void RenderWidgetHostViewMus::RenderProcessGone(base::TerminationStatus status,
                                                int error_code) {
  // TODO(fsamuel): Figure out the interstitial lifetime issues here.
  platform_view_->Destroy();
}

void RenderWidgetHostViewMus::Destroy() {
  if (platform_view_)  // The platform view might have been destroyed already.
    platform_view_->Destroy();
}

gfx::Size RenderWidgetHostViewMus::GetPhysicalBackingSize() const {
  return RenderWidgetHostViewBase::GetPhysicalBackingSize();
}

base::string16 RenderWidgetHostViewMus::GetSelectedText() const {
  return platform_view_->GetSelectedText();
}

void RenderWidgetHostViewMus::SetTooltipText(
    const base::string16& tooltip_text) {
  // TOOD(fsamuel): Ask window manager for tooltip?
}

void RenderWidgetHostViewMus::InitAsChild(gfx::NativeView parent_view) {
  platform_view_->InitAsChild(parent_view);
}

RenderWidgetHost* RenderWidgetHostViewMus::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewMus::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds) {
  // TODO(fsamuel): Implement popups in Mus.
}

void RenderWidgetHostViewMus::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  // TODO(fsamuel): Implement full screen windows in Mus.
}

gfx::NativeView RenderWidgetHostViewMus::GetNativeView() const {
  return gfx::NativeView();
}

gfx::NativeViewId RenderWidgetHostViewMus::GetNativeViewId() const {
  return gfx::NativeViewId();
}

gfx::NativeViewAccessible RenderWidgetHostViewMus::GetNativeViewAccessible() {
  return gfx::NativeViewAccessible();
}

void RenderWidgetHostViewMus::MovePluginWindows(
    const std::vector<WebPluginGeometry>& moves) {
  platform_view_->MovePluginWindows(moves);
}

void RenderWidgetHostViewMus::UpdateCursor(const WebCursor& cursor) {
  // TODO(fsamuel): Implement cursors in Mus.
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMus::SetIsLoading(bool is_loading) {
  platform_view_->SetIsLoading(is_loading);
}

void RenderWidgetHostViewMus::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  // TODO(fsamuel): Implement an IME mojo app.
}

void RenderWidgetHostViewMus::ImeCancelComposition() {
  // TODO(fsamuel): Implement an IME mojo app.
}

void RenderWidgetHostViewMus::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  // TODO(fsamuel): Implement IME.
}

void RenderWidgetHostViewMus::SelectionChanged(const base::string16& text,
                                               size_t offset,
                                               const gfx::Range& range) {
  platform_view_->SelectionChanged(text, offset, range);
}

void RenderWidgetHostViewMus::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  // TODO(fsamuel): Implement selection.
}

void RenderWidgetHostViewMus::SetBackgroundColor(SkColor color) {
  // TODO(fsamuel): Implement background color and opacity.
}

void RenderWidgetHostViewMus::CopyFromCompositingSurface(
    const gfx::Rect& /* src_subrect */,
    const gfx::Size& /* dst_size */,
    const ReadbackRequestCallback& callback,
    const SkColorType /* preferred_color_type */) {
  // TODO(fsamuel): Implement read back.
  NOTIMPLEMENTED();
  callback.Run(SkBitmap(), READBACK_FAILED);
}

void RenderWidgetHostViewMus::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(gfx::Rect(), false);
}

bool RenderWidgetHostViewMus::CanCopyToVideoFrame() const {
  return false;
}

bool RenderWidgetHostViewMus::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  return false;
}

bool RenderWidgetHostViewMus::LockMouse() {
  // TODO(fsamuel): Implement mouse lock in Mus.
  return false;
}

void RenderWidgetHostViewMus::UnlockMouse() {
  // TODO(fsamuel): Implement mouse lock in Mus.
}

void RenderWidgetHostViewMus::GetScreenInfo(blink::WebScreenInfo* results) {
  // TODO(fsamuel): Populate screen info from Mus.
}

bool RenderWidgetHostViewMus::GetScreenColorProfile(
    std::vector<char>* color_profile) {
  // TODO(fsamuel): Implement color profile in Mus.
  return false;
}

gfx::Rect RenderWidgetHostViewMus::GetBoundsInRootWindow() {
  return GetViewBounds();
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewMus::SetActive(bool active) {
  platform_view_->SetActive(active);
}

void RenderWidgetHostViewMus::SetWindowVisibility(bool visible) {
  // TODO(fsamuel): Propagate visibility to Mus?
  platform_view_->SetWindowVisibility(visible);
}

void RenderWidgetHostViewMus::WindowFrameChanged() {
  platform_view_->WindowFrameChanged();
}

void RenderWidgetHostViewMus::ShowDefinitionForSelection() {
  // TODO(fsamuel): Implement this on Mac.
}

bool RenderWidgetHostViewMus::SupportsSpeech() const {
  // TODO(fsamuel): Implement this on mac.
  return false;
}

void RenderWidgetHostViewMus::SpeakSelection() {
  // TODO(fsamuel): Implement this on Mac.
}

bool RenderWidgetHostViewMus::IsSpeaking() const {
  // TODO(fsamuel): Implement this on Mac.
  return false;
}

void RenderWidgetHostViewMus::StopSpeaking() {
  // TODO(fsamuel): Implement this on Mac.
}

bool RenderWidgetHostViewMus::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  return false;
}

#endif  // defined(OS_MACOSX)

void RenderWidgetHostViewMus::LockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMus::UnlockCompositingSurface() {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void RenderWidgetHostViewMus::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {}

gfx::NativeViewId RenderWidgetHostViewMus::GetParentForWindowlessPlugin()
    const {
  return gfx::NativeViewId();
}
#endif

}  // namespace content
