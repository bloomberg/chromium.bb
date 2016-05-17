// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_SOURCE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_SOURCE_VIEW_H_

#include "content/public/browser/desktop_media_id.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

class DesktopMediaListView;

// View used for each item in DesktopMediaListView. Shows a single desktop media
// source as a thumbnail with the title under it.
class DesktopMediaSourceView : public views::View {
 public:
  DesktopMediaSourceView(DesktopMediaListView* parent,
                         content::DesktopMediaID source_id);
  ~DesktopMediaSourceView() override;

  // Updates thumbnail and title from |source|.
  void SetName(const base::string16& name);
  void SetThumbnail(const gfx::ImageSkia& thumbnail);

  // Id for the source shown by this View.
  const content::DesktopMediaID& source_id() const { return source_id_; }

  // Returns true if the source is selected.
  bool is_selected() const { return selected_; }

  // views::View interface.
  const char* GetClassName() const override;
  void Layout() override;
  views::View* GetSelectedViewForGroup(int group) override;
  bool IsGroupFocusTraversable() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  static const char* kDesktopMediaSourceViewClassName;

 private:
  // Updates selection state of the element. If |selected| is true then also
  // calls SetSelected(false) for the source view that was selected before that
  // (if any).
  void SetSelected(bool selected);

  DesktopMediaListView* parent_;
  content::DesktopMediaID source_id_;

  views::ImageView* image_view_;
  views::Label* label_;

  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaSourceView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_SOURCE_VIEW_H_
