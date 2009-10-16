// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_ACTION_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_ACTION_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class Rect;
}

class ExtensionAction {
 public:
  ExtensionAction();
  virtual ~ExtensionAction();

  typedef enum {
    PAGE_ACTION = 0,
    BROWSER_ACTION = 1,
  } ExtensionActionType;

  std::string id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  ExtensionActionType type() const { return type_; }
  void set_type(ExtensionActionType type) { type_ = type; }

  std::string extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  std::string title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  const std::vector<std::string>& icon_paths() const { return icon_paths_; }
  void AddIconPath(const std::string& icon_path) {
    icon_paths_.push_back(icon_path);
  }

  const GURL& popup_url() const { return popup_url_; }
  void set_popup_url(const GURL& url) { popup_url_ = url; }

  bool is_popup() const { return !popup_url_.is_empty(); }

 private:
  // The id for the ExtensionAction, for example: "RssPageAction".
  // For BrowserActions this is blank.
  std::string id_;

  // The type of the ExtensionAction, either PageAction or BrowserAction.
  ExtensionActionType type_;

  // The id for the extension this ExtensionAction belongs to (as defined in
  // the extension manifest).
  std::string extension_id_;

  // The title text of the ExtensionAction.
  std::string title_;

  // The paths to the icons that this PageIcon can show.
  std::vector<std::string> icon_paths_;

  // If the action has a popup, it has a URL and a height.
  GURL popup_url_;
};

typedef std::map<std::string, ExtensionAction*> ExtensionActionMap;

// This class keeps track of what values each tab uses to override the default
// values of the ExtensionAction.
class ExtensionActionState {
 public:
  ExtensionActionState(std::string title, int icon_index)
    : title_(title), icon_index_(icon_index),
      badge_background_color_(SkColorSetARGB(255, 218, 0, 24)) {
  }

  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  const std::string& badge_text() const { return badge_text_; }
  void set_badge_text(const std::string& badge_text) {
    badge_text_ = badge_text;
  }

  SkColor badge_background_color() const {
    return badge_background_color_;
  }
  void set_badge_background_color(SkColor badge_background_color) {
    badge_background_color_ = badge_background_color;
  }

  int icon_index() const { return icon_index_; }
  void set_icon_index(int icon_index) { icon_index_ = icon_index; }

  SkBitmap* icon() const { return icon_.get(); }
  void set_icon(SkBitmap* icon) { icon_.reset(icon); }

  void PaintBadge(gfx::Canvas* canvas, const gfx::Rect& bounds);

 private:
  // The title text to use for tooltips and labels.
  std::string title_;

  // The icon to use.
  int icon_index_;

  // If non-NULL, overrides icon_index.
  scoped_ptr<SkBitmap> icon_;

    // The badge text.
  std::string badge_text_;

  // The background color for the badge.
  SkColor badge_background_color_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionState);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_ACTION_H_
