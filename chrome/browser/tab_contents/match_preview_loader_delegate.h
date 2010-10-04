// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_LOADER_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_LOADER_DELEGATE_H_
#pragma once

#include "base/string16.h"

namespace gfx {
class Rect;
}

class MatchPreviewLoader;

// MatchPreviewLoader's delegate. This interface is implemented by
// MatchPreview.
class MatchPreviewLoaderDelegate {
 public:
  // Invoked when the loader is ready to be shown.
  virtual void ShowMatchPreviewLoader(MatchPreviewLoader* loader) = 0;

  // Invoked when the renderer has suggested text.
  virtual void SetSuggestedTextFor(MatchPreviewLoader* loader,
                                   const string16& text) = 0;

  // Returns the bounds of the match preview.
  virtual gfx::Rect GetMatchPreviewBounds() = 0;

  // Returns true if preview should be committed on mouse up.
  virtual bool ShouldCommitPreviewOnMouseUp() = 0;

  // Invoked when the preview should be committed.
  virtual void CommitPreview(MatchPreviewLoader* loader) = 0;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_LOADER_DELEGATE_H_
