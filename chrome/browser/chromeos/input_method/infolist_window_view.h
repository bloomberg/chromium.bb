// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_VIEW_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/views/view.h"

namespace gfx {
class Font;
}

namespace chromeos {
namespace input_method {

class InfolistEntryView;

// InfolistWindowView is the main container of the infolist window UI.
class InfolistWindowView : public views::View {
 public:
  // Represents an infolist entry.
  struct Entry {
    std::string title;
    std::string body;
  };

  InfolistWindowView();
  virtual ~InfolistWindowView();
  void Init();

  // Updates infolist contents with |entries| and |focused_index|. If you want
  // to unselect all entries, pass |focused_index| as InvalidFocusIndex().
  void Relayout(const std::vector<Entry>& entries, size_t focused_index);

  // Returns a index taht is invalid.
  static const size_t InvalidFocusIndex();

 private:
  // The infolist area is where the meanings and the usages of the words are
  // rendered.
  views::View* infolist_area_;

  // The infolist views are used for rendering the meanings and the usages of
  // the words.
  ScopedVector<InfolistEntryView> infolist_views_;

  // Information title font
  scoped_ptr<gfx::Font> title_font_;

  // Information description font
  scoped_ptr<gfx::Font> description_font_;

  DISALLOW_COPY_AND_ASSIGN(InfolistWindowView);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_VIEW_H_
