// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
#define CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_

#include <list>
#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/media_stream_request.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link_listener.h"

class ContentSettingBubbleModel;

namespace chrome {
class ContentSettingBubbleViewsBridge;
}

namespace views {
class RadioButton;
class LabelButton;
}

// ContentSettingBubbleContents is used when the user turns on different kinds
// of content blocking (e.g. "block images").  When viewing a page with blocked
// content, icons appear in the omnibox corresponding to the content types that
// were blocked, and the user can click one to get a bubble hosting a few
// controls.  This class provides the content of that bubble.  In general,
// these bubbles typically have a title, a pair of radio buttons for toggling
// the blocking settings for the current site, a close button, and a link to
// get to a more comprehensive settings management dialog.  A few types have
// more or fewer controls than this.
class ContentSettingBubbleContents : public content::WebContentsObserver,
                                     public views::BubbleDialogDelegateView,
                                     public views::ButtonListener,
                                     public views::LinkListener,
                                     public views::ComboboxListener {
 public:
  ContentSettingBubbleContents(
      ContentSettingBubbleModel* content_setting_bubble_model,
      content::WebContents* web_contents,
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow);
  ~ContentSettingBubbleContents() override;

  gfx::Size GetPreferredSize() const override;

 protected:
  // views::BubbleDialogDelegateView:
  void Init() override;
  View* CreateExtraView() override;
  bool Accept() override;
  bool Close() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

 private:
  // A combobox model that builds the contents of the media capture devices menu
  // in the content setting bubble.
  class MediaComboboxModel : public ui::ComboboxModel {
   public:
    explicit MediaComboboxModel(content::MediaStreamType type);
    ~MediaComboboxModel() override;

    content::MediaStreamType type() const { return type_; }
    const content::MediaStreamDevices& GetDevices() const;
    int GetDeviceIndex(const content::MediaStreamDevice& device) const;

    // ui::ComboboxModel:
    int GetItemCount() const override;
    base::string16 GetItemAt(int index) override;

   private:
    content::MediaStreamType type_;

    DISALLOW_COPY_AND_ASSIGN(MediaComboboxModel);
  };

  class Favicon;

  // This allows ContentSettingBubbleViewsBridge to call SetAnchorRect().
  friend class chrome::ContentSettingBubbleViewsBridge;

  typedef std::map<views::Link*, int> ListItemLinks;

  // content::WebContentsObserver:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::ComboboxListener:
  void OnPerformAction(views::Combobox* combobox) override;

  // Provides data for this bubble.
  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model_;

  // Some of our controls, so we can tell what's been clicked when we get a
  // message.
  ListItemLinks list_item_links_;
  typedef std::vector<views::RadioButton*> RadioGroup;
  RadioGroup radio_group_;
  views::Link* custom_link_;
  views::Link* manage_link_;
  views::LabelButton* manage_button_;
  views::Link* learn_more_link_;

  // Combobox models the bubble owns.
  std::list<MediaComboboxModel> combobox_models_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingBubbleContents);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
