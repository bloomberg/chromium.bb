// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper function which takes a custom thumbnail for the client-side phishing
// detector.
//
// Important note: in some circumstances grabbing a thumbnail using
// this function can be very slow since it may need to re-layout the page
// twice. Also, using this class may have some side effects (e.g.,
// onScroll and onResize will be called, etc).  Currently, this function
// is only used if Chrome is almost certain that the current page is
// phishing (according to the client-side phishing detector) in which
// case we are not too worried about performance or possibly causing
// some JavaScript weirdness on the page.

#ifndef CHROME_RENDERER_SAFE_BROWSING_PHISHING_THUMBNAILER_H_
#define CHROME_RENDERER_SAFE_BROWSING_PHISHING_THUMBNAILER_H_
#pragma once

namespace gfx {
class Size;
}
class RenderView;
class SkBitmap;

namespace safe_browsing {

// Grabs a thumbnail returns a bitmap that contains the result.  Before grabbing
// a snapshot the view will be re-sized to |view_size| and the resulting
// snapshot will then be re-sized to the given |thumbnail_size|.  If grabbing
// the thumbnail fails this function returns SkBitmap() in which case calling
// isNull() on the returned bitmap will return true.
SkBitmap GrabPhishingThumbnail(RenderView* render_view,
                               const gfx::Size& view_size,
                               const gfx::Size& thumbnail_size);

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_PHISHING_THUMBNAILER_H_
