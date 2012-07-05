// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu.h"

#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"

using extensions::MenuItem;

class RenderViewContextMenuTest : public testing::Test {
 public:
  RenderViewContextMenuTest() { }

 protected:
  // Proxy defined here to minimize friend classes in RenderViewContextMenu
  static bool ExtensionContextAndPatternMatch(
      const content::ContextMenuParams& params,
      MenuItem::ContextList contexts,
      const URLPatternSet& patterns) {
    return RenderViewContextMenu::ExtensionContextAndPatternMatch(params,
        contexts, patterns);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuTest);
};

// Generates a ContextMenuParams that matches the specified contexts.
static content::ContextMenuParams CreateParams(int contexts) {
  content::ContextMenuParams rv;
  rv.is_editable = false;
  rv.media_type = WebKit::WebContextMenuData::MediaTypeNone;
  rv.page_url = GURL("http://test.page/");

  static const char16 selected_text[] = { 's', 'e', 'l', 0 };
  if (contexts & MenuItem::SELECTION)
    rv.selection_text = selected_text;

  if (contexts & MenuItem::LINK)
    rv.link_url = GURL("http://test.link/");

  if (contexts & MenuItem::EDITABLE)
    rv.is_editable = true;

  if (contexts & MenuItem::IMAGE) {
    rv.src_url = GURL("http://test.image/");
    rv.media_type = WebKit::WebContextMenuData::MediaTypeImage;
  }

  if (contexts & MenuItem::VIDEO) {
    rv.src_url = GURL("http://test.video/");
    rv.media_type = WebKit::WebContextMenuData::MediaTypeVideo;
  }

  if (contexts & MenuItem::AUDIO) {
    rv.src_url = GURL("http://test.audio/");
    rv.media_type = WebKit::WebContextMenuData::MediaTypeAudio;
  }

  if (contexts & MenuItem::FRAME)
    rv.frame_url = GURL("http://test.frame/");

  return rv;
}

// Generates a URLPatternSet with a single pattern
static URLPatternSet CreatePatternSet(const std::string& pattern) {
  URLPattern target(URLPattern::SCHEME_HTTP);
  target.Parse(pattern);

  URLPatternSet rv;
  rv.AddPattern(target);

  return rv;
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForPage) {
  content::ContextMenuParams params = CreateParams(0);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForLink) {
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::LINK);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForImage) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForVideo) {
  content::ContextMenuParams params = CreateParams(MenuItem::VIDEO);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::VIDEO);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForAudio) {
  content::ContextMenuParams params = CreateParams(MenuItem::AUDIO);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::AUDIO);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, MatchWhenLinkedImageMatchesTarget) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE |
                                                   MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::LINK);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.link/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, MatchWhenLinkedImageMatchesSource) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE |
                                                   MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::LINK);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.image/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, NoMatchWhenLinkedImageMatchesNeither) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE |
                                                   MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::LINK);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForFrame) {
  content::ContextMenuParams params = CreateParams(MenuItem::FRAME);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::FRAME);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForEditable) {
  content::ContextMenuParams params = CreateParams(MenuItem::EDITABLE);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::EDITABLE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForSelection) {
  content::ContextMenuParams params =
      CreateParams(MenuItem::SELECTION);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::SELECTION);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForSelectionOnLink) {
  content::ContextMenuParams params = CreateParams(
      MenuItem::SELECTION | MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::SELECTION);
  contexts.Add(MenuItem::LINK);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForSelectionOnImage) {
  content::ContextMenuParams params = CreateParams(
      MenuItem::SELECTION | MenuItem::IMAGE);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::SELECTION);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}
