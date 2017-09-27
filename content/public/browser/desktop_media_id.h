// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_

#include <string>
#include <tuple>

#include "content/common/content_export.h"
#include "content/public/browser/web_contents_media_capture_id.h"

#if defined(USE_AURA)
namespace aura {
class Window;
}  // namespace aura
#endif  // defined(USE_AURA)

namespace content {

// Type used to identify desktop media sources. It's converted to string and
// stored in MediaStreamRequest::requested_video_device_id.
struct CONTENT_EXPORT DesktopMediaID {
 public:
  enum Source {
    SOURCE_NONE,
    SOURCE_SCREEN,
    SOURCE_WINDOW,
    SOURCE_WEB_CONTENTS
  };
  enum Capture { CAPTURE_VIDEO = 1, CAPTURE_AUDIO = 2 };

  typedef intptr_t Id;

  // Represents an "unset" value for either |id| or |aura_id|.
  static const Id kNullId = 0;

#if defined(USE_AURA)
  // Assigns integer identifier to the |window| and returns its DesktopMediaID.
  static DesktopMediaID RegisterAuraWindow(Source source_type,
                                           aura::Window* window);

  // Returns the Window that was previously registered using
  // RegisterAuraWindow(), else nullptr.
  static aura::Window* GetAuraWindowById(const DesktopMediaID& id);
#endif  // defined(USE_AURA)

  DesktopMediaID() = default;

  DesktopMediaID(Source source_type, Id id)
      : source_type(source_type), id(id) {}

  DesktopMediaID(Source source_type,
                 Id id,
                 WebContentsMediaCaptureId web_contents_id)
      : source_type(source_type), id(id), web_contents_id(web_contents_id) {}

  DesktopMediaID(Source source_type, Id id, uint32_t capture_type)
      : source_type(source_type), id(id), capture_type(capture_type) {}

  // Operators so that DesktopMediaID can be used with STL containers.
  bool operator<(const DesktopMediaID& other) const;
  bool operator==(const DesktopMediaID& other) const;

  bool is_null() const { return source_type == SOURCE_NONE; }
  void set_video_capture(bool capture);
  void set_audio_capture(bool capture);
  bool is_video_capture() const { return capture_type & CAPTURE_VIDEO; }
  bool is_audio_capture() const { return capture_type & CAPTURE_AUDIO; }
  static DesktopMediaID Parse(const std::string& str);
  std::string ToString() const;

  Source source_type = SOURCE_NONE;

  // The IDs referring to the target native screen or window.  |id| will be
  // non-null if and only if it refers to a native screen/window.  |aura_id|
  // will be non-null if and only if it refers to an Aura window.  Note that is
  // it possible for both of these to be non-null, which means both IDs are
  // referring to the same logical window.
  Id id = kNullId;
#if defined(USE_AURA)
  // TODO(miu): Make this an int, after clean-up for http://crbug.com/513490.
  Id aura_id = kNullId;
#endif

  // This bitmask of Capture values records the type of media captured.
  uint32_t capture_type = CAPTURE_VIDEO;

  // This id contains information for WebContents capture.
  WebContentsMediaCaptureId web_contents_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_
