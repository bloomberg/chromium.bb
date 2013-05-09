// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/in_process_renderer/in_process_view_renderer.h"

#include "android_webview/public/browser/draw_gl.h"
#include "base/logging.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/renderer/android/synchronous_compositor.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {

namespace {
const void* kUserDataKey = &kUserDataKey;

class UserData : public content::WebContents::Data {
 public:
  UserData(InProcessViewRenderer* ptr) : instance_(ptr) {}
  virtual ~UserData() {
    instance_->WebContentsGone();
  }

  static InProcessViewRenderer* GetInstance(content::WebContents* contents) {
    if (!contents)
      return NULL;
    UserData* data = reinterpret_cast<UserData*>(
        contents->GetUserData(kUserDataKey));
    return data ? data->instance_ : NULL;
  }

 private:
  InProcessViewRenderer* instance_;
};

}  // namespace

InProcessViewRenderer::InProcessViewRenderer(
    BrowserViewRenderer::Client* client,
    JavaHelper* java_helper)
    : web_contents_(NULL),
      compositor_(NULL),
      client_(client),
      view_visible_(false),
      inside_draw_(false),
      continuous_invalidate_(false),
      width_(0),
      height_(0),
      attached_to_window_(false),
      hardware_initialized_(false),
      hardware_failed_(false),
      egl_context_at_init_(NULL) {
}

InProcessViewRenderer::~InProcessViewRenderer() {
  if (compositor_)
    compositor_->SetClient(NULL);
  SetContents(NULL);
}

// static
InProcessViewRenderer* InProcessViewRenderer::FromWebContents(
    content::WebContents* contents) {
  return UserData::GetInstance(contents);
}

// static
InProcessViewRenderer* InProcessViewRenderer::FromId(int render_process_id,
                                                     int render_view_id) {
  const content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!rvh) return NULL;
  return InProcessViewRenderer::FromWebContents(
      content::WebContents::FromRenderViewHost(rvh));
}

void InProcessViewRenderer::BindSynchronousCompositor(
    content::SynchronousCompositor* compositor) {
  DCHECK(compositor && compositor_ != compositor);
  if (compositor_)
    compositor_->SetClient(NULL);
  compositor_ = compositor;
  hardware_initialized_ = false;
  hardware_failed_ = false;
  compositor_->SetClient(this);

  if (attached_to_window_)
    client_->RequestProcessMode();
}

void InProcessViewRenderer::SetContents(
    content::ContentViewCore* content_view_core) {
  // First remove association from the prior ContentViewCore / WebContents.
  if (web_contents_) {
    web_contents_->SetUserData(kUserDataKey, NULL);
    DCHECK(!web_contents_);  // WebContentsGone should have been called.
  }

  if (!content_view_core)
    return;

  web_contents_ = content_view_core->GetWebContents();
  web_contents_->SetUserData(kUserDataKey, new UserData(this));
}

void InProcessViewRenderer::WebContentsGone() {
  web_contents_ = NULL;
}

bool InProcessViewRenderer::RenderPicture(SkCanvas* canvas) {
  return compositor_ && compositor_->DemandDrawSw(canvas);
}

bool InProcessViewRenderer::PrepareDrawGL(int x, int y) {
  // No harm in updating |hw_rendering_scroll_| even if we return false.
  hw_rendering_scroll_ = gfx::Point(x, y);
  return attached_to_window_ && compositor_ && !hardware_failed_;
}

void InProcessViewRenderer::DrawGL(AwDrawGLInfo* draw_info) {
  DCHECK(view_visible_);

  // We need to watch if the current Android context has changed and enforce
  // a clean-up in the compositor.
  EGLContext current_context = eglGetCurrentContext();
  if (!current_context) {
    LOG(WARNING) << "No current context attached. Skipping composite.";
    return;
  }

  if (attached_to_window_ && compositor_ && !hardware_initialized_) {
    // TODO(boliu): Actually initialize the compositor GL path.
    hardware_initialized_ = true;
    egl_context_at_init_ = current_context;
  }

  if (draw_info->mode == AwDrawGLInfo::kModeProcess)
    return;

  if (egl_context_at_init_ != current_context) {
    // TODO(boliu): Handle context lost
  }

  // TODO(boliu): Make sure this is not called before compositor is initialized
  // and GL is ready. Then make this a DCHECK.
  if (!compositor_)
    return;

  // TODO(boliu): Have a scoped var to unset this.
  inside_draw_ = true;

  gfx::Transform transform;
  transform.matrix().setColMajorf(draw_info->transform);
  transform.Translate(hw_rendering_scroll_.x(), hw_rendering_scroll_.y());
  compositor_->DemandDrawHw(
      gfx::Size(draw_info->width, draw_info->height),
      transform,
      gfx::Rect(draw_info->clip_left,
                draw_info->clip_top,
                draw_info->clip_right - draw_info->clip_left,
                draw_info->clip_bottom - draw_info->clip_top));

  inside_draw_ = false;

  // The GL functor must ensure these are set to zero before returning.
  // Not setting them leads to graphical artifacts that can affect other apps.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // TODO(boliu): Should post task to invalidate.
  if (continuous_invalidate_)
    Invalidate();
}

bool InProcessViewRenderer::DrawSW(jobject java_canvas,
                                   const gfx::Rect& clip) {
  return false;
}

base::android::ScopedJavaLocalRef<jobject>
InProcessViewRenderer::CapturePicture() {
  return base::android::ScopedJavaLocalRef<jobject>();
}

void InProcessViewRenderer::EnableOnNewPicture(bool enabled) {
}

void InProcessViewRenderer::OnVisibilityChanged(bool view_visible,
                                                bool window_visible) {
  view_visible_ = window_visible && view_visible;
}

void InProcessViewRenderer::OnSizeChanged(int width, int height) {
  width_ = width;
  height_ = height;
}

void InProcessViewRenderer::OnAttachedToWindow(int width, int height) {
  attached_to_window_ = true;
  width_ = width;
  height_ = height;
  if (compositor_ && !hardware_initialized_)
    client_->RequestProcessMode();
}

void InProcessViewRenderer::OnDetachedFromWindow() {
  // TODO(joth): Release GL resources. crbug.com/231986.
  attached_to_window_ = false;
}

bool InProcessViewRenderer::IsAttachedToWindow() {
  return attached_to_window_;
}

bool InProcessViewRenderer::IsViewVisible() {
  return view_visible_;
}

gfx::Rect InProcessViewRenderer::GetScreenRect() {
  return gfx::Rect();
}

void InProcessViewRenderer::DidDestroyCompositor(
    content::SynchronousCompositor* compositor) {
  // Allow for transient hand-over when two compositors may reference
  // a single client.
  if (compositor_ == compositor)
    compositor_ = NULL;
}

void InProcessViewRenderer::SetContinuousInvalidate(bool invalidate) {
  if (continuous_invalidate_ == invalidate)
    return;

  continuous_invalidate_ = invalidate;
  // TODO(boliu): Handle if not attached to window case.
  if (continuous_invalidate_ && !inside_draw_)
    Invalidate();
}

void InProcessViewRenderer::Invalidate() {
  DCHECK(view_visible_);
  client_->Invalidate();
}

}  // namespace android_webview
