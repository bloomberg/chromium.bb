// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_TAB_FINDER_H_
#define CHROME_RENDERER_EXTENSIONS_TAB_FINDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/renderer/render_view_visitor.h"

namespace content {
class RenderView;
}

namespace extensions {

// Finds the top RenderView associated with a tab ID.
class TabFinder : public content::RenderViewVisitor {
 public:
  // Returns the top RenderView with |tab_id|, or NULL if none is found.
  static content::RenderView* Find(int tab_id);

 private:
  explicit TabFinder(int tab_id);
  virtual ~TabFinder();

  // content::RenderViewVisitor implementation.
  virtual bool Visit(content::RenderView* render_view) OVERRIDE;

  int tab_id_;
  content::RenderView* view_;

  DISALLOW_COPY_AND_ASSIGN(TabFinder);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_TAB_FINDER_H_
