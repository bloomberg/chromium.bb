// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profile_menu_model.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"
#include "views/widget/widget.h"

namespace {

const int kCellWidth = 32;
const int kCellHeight = 32;
const int kCellPaddingX = 5;
const int kCellPaddingY = 5;
const int kGridMaxCol = 3;

static inline int Round(double x) {
  return static_cast<int>(x + 0.5);
}

// This is an button that scales its image to fit its bounds.
class ScaledImageButton : public views::ImageButton {
 public:
  explicit ScaledImageButton(views::ButtonListener* listener);
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScaledImageButton);
};

// A view that displays avatar icons in a grid.
class AvatarIconGridView : public views::View, public views::ButtonListener {
 public:
  AvatarIconGridView(Profile* profile, views::MenuItemView* menu);
  virtual ~AvatarIconGridView();

  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

 private:
  // Sets the profile's avatar icon to the icon at the given index.
  void SetAvatarIconToIndex(int icon_index);

  typedef std::map<int, views::View*> IconIndexToButtonMap;
  IconIndexToButtonMap icon_index_to_button_map_;
  Profile* profile_;
  views::MenuItemView* menu_;

  DISALLOW_COPY_AND_ASSIGN(AvatarIconGridView);
};

ScaledImageButton::ScaledImageButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
}

void ScaledImageButton::OnPaint(gfx::Canvas* canvas) {
  if (state() == views::CustomButton::BS_HOT)
    canvas->FillRectInt(SkColorSetARGB(20, 0, 0, 0), 0, 0, width(), height());
  else if (state() == views::CustomButton::BS_PUSHED)
    canvas->FillRectInt(SkColorSetARGB(40, 0, 0, 0), 0, 0, width(), height());

  const SkBitmap& icon = GetImageToPaint();
  if (icon.isNull())
    return;

  int dst_width;
  int dst_height;
  if (icon.width() > icon.height()) {
    dst_width = std::min(width(), icon.width());
    float scale = static_cast<float>(dst_width) /
                  static_cast<float>(icon.width());
    dst_height = Round(icon.height() * scale);
  } else {
    dst_height = std::min(height(), icon.height());
    float scale = static_cast<float>(dst_height) /
                  static_cast<float>(icon.height());
    dst_width = Round(icon.width() * scale);
  }
  int dst_x = (width() - dst_width) / 2;
  int dst_y = (height() - dst_height) / 2;
  canvas->DrawBitmapInt(icon, 0, 0, icon.width(), icon.height(),
                        dst_x, dst_y, dst_width, dst_height, false);
}

AvatarIconGridView::AvatarIconGridView(Profile* profile,
                                       views::MenuItemView* menu)
    : profile_(profile),
      menu_(menu) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  for (size_t i = 0; i < ProfileInfoCache::GetDefaultAvatarIconCount(); ++i) {
    int resource_id =
        ProfileInfoCache::GetDefaultAvatarIconResourceIDAtIndex(i);
    views::ImageButton* button = new ScaledImageButton(this);
    button->SetImage(views::CustomButton::BS_NORMAL,
                     rb.GetImageNamed(resource_id));
    button->SetAccessibleName(
        l10n_util::GetStringFUTF16Int(IDS_NUMBERED_AVATAR_NAME,
                                      static_cast<int>(i + 1)));
    AddChildView(button);
    icon_index_to_button_map_[i] = button;
  }
}

AvatarIconGridView::~AvatarIconGridView() {
}

void AvatarIconGridView::Layout() {
  for (IconIndexToButtonMap::const_iterator it =
       icon_index_to_button_map_.begin();
       it != icon_index_to_button_map_.end(); ++it) {
    views::View* view = it->second;
    int icon_index = it->first;
    int col = icon_index % kGridMaxCol;
    int row = icon_index / kGridMaxCol;
    int x = col * kCellWidth + col * kCellPaddingX;
    int y = row * kCellHeight + row * kCellPaddingY;
    view->SetBounds(x, y, kCellWidth, kCellHeight);
  }
}

gfx::Size AvatarIconGridView::GetPreferredSize() {
  int cols = kGridMaxCol;
  int rows = ProfileInfoCache::GetDefaultAvatarIconCount() / cols;
  if (ProfileInfoCache::GetDefaultAvatarIconCount() % cols != 0)
    rows++;
  return gfx::Size(cols * kCellWidth + (cols - 1) * kCellPaddingX,
                   rows * kCellHeight + (rows - 1) * kCellPaddingY);
}

void AvatarIconGridView::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  for (IconIndexToButtonMap::const_iterator it =
       icon_index_to_button_map_.begin();
       it != icon_index_to_button_map_.end(); ++it) {
    if (it->second == sender) {
      SetAvatarIconToIndex(it->first);
      menu_->Cancel();
      return;
    }
  }
  NOTREACHED();
}

void AvatarIconGridView::SetAvatarIconToIndex(int icon_index) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
  cache.SetAvatarIconOfProfileAtIndex(profile_index, icon_index);
}

} // namespace

AvatarMenu::AvatarMenu(ui::MenuModel* model, Profile* profile)
    : MenuModelAdapter(model),
      profile_(profile) {
  root_.reset(new views::MenuItemView(this));
  BuildMenu(root_.get());
  views::MenuItemView* item =
      root_->GetMenuItemByID(ProfileMenuModel::COMMAND_CHOOSE_AVATAR_ICON);
  item->AddChildView(new AvatarIconGridView(profile_, root_.get()));
}

AvatarMenu::~AvatarMenu() {
}

void AvatarMenu::RunMenu(views::MenuButton* host) {
  // Up the ref count while the menu is displaying. This way if the window is
  // deleted while we're running we won't prematurely delete the menu.
  // TODO(sky): fix this, the menu should really take ownership of the menu
  // (57890).
  scoped_refptr<AvatarMenu> dont_delete_while_running(this);
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(host, &screen_loc);
  gfx::Rect bounds(screen_loc, host->size());
  root_->RunMenuAt(host->GetWidget(), host, bounds,
                   views::MenuItemView::TOPLEFT, true);
}
