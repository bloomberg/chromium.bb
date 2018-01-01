// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_utils.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/lazy_instance.h"
#include "base/mac/scoped_objc_class_swizzler.h"
#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/text_input_client_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

// The interface class used to override the implementation of some of
// RenderWidgetHostViewCocoa methods for tests.
@interface RenderWidgetHostViewCocoaSwizzler : NSObject
- (void)didAddSubview:(NSView*)view;
@end

namespace content {

using base::mac::ScopedObjCClassSwizzler;

// static
const char* RenderWidgetHostViewCocoaObserver::kDidAddSubview = "didAddSubview";

// static
std::map<std::string, std::unique_ptr<base::mac::ScopedObjCClassSwizzler>>
    RenderWidgetHostViewCocoaObserver::rwhvcocoa_swizzlers_;

// static
std::map<WebContents*, RenderWidgetHostViewCocoaObserver*>
    RenderWidgetHostViewCocoaObserver::observers_;

namespace {

content::RenderWidgetHostViewMac* GetRenderWidgetHostViewMac(NSObject* object) {
  for (auto* contents : WebContentsImpl::GetAllWebContents()) {
    if (!contents->GetBrowserPluginGuest()) {
      RenderWidgetHostViewMac* rwhv_mac = static_cast<RenderWidgetHostViewMac*>(
          contents->GetRenderWidgetHostView());
      if (rwhv_mac->cocoa_view() == object)
        return rwhv_mac;
    }
  }

  return nullptr;
}

}  // namespace

base::mac::ScopedObjCClassSwizzler*
RenderWidgetHostViewCocoaObserver::GetSwizzler(const std::string& method_name) {
  return rwhvcocoa_swizzlers_.count(method_name)
             ? rwhvcocoa_swizzlers_.at(method_name).get()
             : nullptr;
}

// static
RenderWidgetHostViewCocoaObserver*
RenderWidgetHostViewCocoaObserver::GetObserver(WebContents* web_contents) {
  return observers_.count(web_contents) ? observers_.at(web_contents) : nullptr;
}

RenderWidgetHostViewCocoaObserver::RenderWidgetHostViewCocoaObserver(
    WebContents* web_contents)
    : web_contents_(web_contents) {
  if (rwhvcocoa_swizzlers_.empty())
    SetUpSwizzlers();

  DCHECK(!observers_.count(web_contents));
  observers_[web_contents] = this;
}

RenderWidgetHostViewCocoaObserver::~RenderWidgetHostViewCocoaObserver() {
  observers_.erase(web_contents_);

  if (observers_.empty())
    rwhvcocoa_swizzlers_.clear();
}

void RenderWidgetHostViewCocoaObserver::SetUpSwizzlers() {
  if (!rwhvcocoa_swizzlers_.empty())
    return;

  // [RenderWidgetHostViewCocoa didAddSubview:NSView*].
  SEL selector = NSSelectorFromString([NSString
      stringWithUTF8String:base::StringPrintf("%s:", kDidAddSubview).c_str()]);
  rwhvcocoa_swizzlers_[kDidAddSubview] =
      std::make_unique<ScopedObjCClassSwizzler>(
          GetRenderWidgetHostViewCocoaClassForTesting(),
          [RenderWidgetHostViewCocoaSwizzler class], selector);
}

void SetWindowBounds(gfx::NativeWindow window, const gfx::Rect& bounds) {
  NSRect new_bounds = NSRectFromCGRect(bounds.ToCGRect());
  if ([[NSScreen screens] count] > 0) {
    new_bounds.origin.y =
        [[[NSScreen screens] firstObject] frame].size.height -
        new_bounds.origin.y - new_bounds.size.height;
  }

  [window setFrame:new_bounds display:NO];
}

void GetStringAtPointForRenderWidget(
    RenderWidgetHost* rwh,
    const gfx::Point& point,
    base::Callback<void(const std::string&, const gfx::Point&)>
        result_callback) {
  TextInputClientMac::GetInstance()->GetStringAtPoint(
      rwh, point, ^(NSAttributedString* string, NSPoint baselinePoint) {
        result_callback.Run([[string string] UTF8String],
                            gfx::Point(baselinePoint.x, baselinePoint.y));
      });
}

void GetStringFromRangeForRenderWidget(
    RenderWidgetHost* rwh,
    const gfx::Range& range,
    base::Callback<void(const std::string&, const gfx::Point&)>
        result_callback) {
  TextInputClientMac::GetInstance()->GetStringFromRange(
      rwh, range.ToNSRange(),
      ^(NSAttributedString* string, NSPoint baselinePoint) {
        result_callback.Run([[string string] UTF8String],
                            gfx::Point(baselinePoint.x, baselinePoint.y));
      });
}

}  // namespace content

@implementation RenderWidgetHostViewCocoaSwizzler
- (void)didAddSubview:(NSView*)view {
  content::RenderWidgetHostViewCocoaObserver::GetSwizzler(
      content::RenderWidgetHostViewCocoaObserver::kDidAddSubview)
      ->GetOriginalImplementation()(self, _cmd, view);

  content::RenderWidgetHostViewMac* rwhv_mac =
      content::GetRenderWidgetHostViewMac(self);

  if (!rwhv_mac)
    return;

  content::RenderWidgetHostViewCocoaObserver* observer =
      content::RenderWidgetHostViewCocoaObserver::GetObserver(
          rwhv_mac->GetWebContents());

  if (!observer)
    return;

  NSRect bounds_in_cocoa_view =
      [view convertRect:view.bounds toView:rwhv_mac->cocoa_view()];

  gfx::Rect rect =
      [rwhv_mac->cocoa_view() flipNSRectToRect:bounds_in_cocoa_view];

  observer->DidAddSubviewWillBeDismissed(rect);

  // This override is useful for testing popups. To make sure the run loops end
  // after the call it is best to dismiss the popup soon.
  NSEvent* dismissal_event =
      [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                         location:NSZeroPoint
                    modifierFlags:0
                        timestamp:0.0
                     windowNumber:0
                          context:nil
                      eventNumber:0
                       clickCount:1
                         pressure:1.0];
  [[NSApplication sharedApplication] postEvent:dismissal_event atStart:false];
}
@end
