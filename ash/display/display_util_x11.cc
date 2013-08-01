// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_util_x11.h"

#include <algorithm>
#include <map>
#include <X11/extensions/Xrandr.h>

#include "ash/display/display_info.h"
#include "base/logging.h"
#include "chromeos/display/output_util.h"

namespace ash {
namespace internal {
namespace {

// A list of bogus sizes in mm that X detects that should be ignored.
// See crbug.com/136533. The first element maintains the minimum
// size required to be valid size.
const unsigned long kInvalidDisplaySizeList[][2] = {
  {40, 30},
  {50, 40},
  {160, 90},
  {160, 100},
};

// Resolution list are sorted by the area in pixels and the larger
// one comes first.
struct ResolutionSorter {
  bool operator()(const Resolution& a, const Resolution& b) {
    return a.size.width() * a.size.height() > b.size.width() * b.size.height();
  }
};

}  // namespace

bool ShouldIgnoreSize(unsigned long mm_width, unsigned long mm_height) {
  // Ignore if the reported display is smaller than minimum size.
  if (mm_width <= kInvalidDisplaySizeList[0][0] ||
      mm_height <= kInvalidDisplaySizeList[0][1]) {
    LOG(WARNING) << "Smaller than minimum display size";
    return true;
  }
  for (unsigned long i = 1 ; i < arraysize(kInvalidDisplaySizeList); ++i) {
    const unsigned long* size = kInvalidDisplaySizeList[i];
    if (mm_width == size[0] && mm_height == size[1]) {
      LOG(WARNING) << "Black listed display size detected:"
                   << size[0] << "x" << size[1];
      return true;
    }
  }
  return false;
}

std::vector<Resolution> GetResolutionList(
    XRRScreenResources* screen_resources,
    XRROutputInfo* output_info) {
  typedef std::map<std::pair<int,int>, Resolution> ResolutionMap;

  ResolutionMap resolution_map;

  for (int i = 0; i < output_info->nmode; i++) {
    RRMode mode = output_info->modes[i];
    const XRRModeInfo* info = chromeos::FindModeInfo(screen_resources, mode);
    DCHECK(info);
    // Just ignore bad entry on Release build.
    if (!info)
      continue;
    ResolutionMap::key_type size = std::make_pair(info->width, info->height);
    bool interlaced = (info->modeFlags & RR_Interlace) != 0;

    ResolutionMap::iterator iter = resolution_map.find(size);

    // Add new resolution if it's new size or override interlaced mode.
    if (iter == resolution_map.end()) {
      resolution_map.insert(ResolutionMap::value_type(
          size,
          Resolution(gfx::Size(info->width, info->height), interlaced)));
    } else if (iter->second.interlaced && !interlaced) {
      iter->second.interlaced = false;
    }
  }

  std::vector<Resolution> resolution_list;
  for (ResolutionMap::const_iterator iter = resolution_map.begin();
       iter != resolution_map.end();
       ++iter) {
    resolution_list.push_back(iter->second);
  }
  std::sort(resolution_list.begin(), resolution_list.end(), ResolutionSorter());
  return resolution_list;
}

}  // namespace internal
}  // namespace ash
