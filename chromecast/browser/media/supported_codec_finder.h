// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace chromecast {
namespace media {

class MediaCapsImpl;

class SupportedCodecFinder {
 public:
  // Notifies the given MediaCaps of all found supported codecs.
  void FindSupportedCodecProfileLevels(MediaCapsImpl* media_caps);
};

}  // namespace media
}  // namespace chromecast
