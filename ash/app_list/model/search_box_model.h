// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_BOX_MODEL_H_
#define ASH_APP_LIST_MODEL_SEARCH_BOX_MODEL_H_

#include <memory>

#include "ash/app_list/model/app_list_model_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/selection_model.h"

namespace app_list {

class SearchBoxModelObserver;

// SearchBoxModel consisits of an icon, a hint text, a user text and a selection
// model. The icon is rendered to the side of the query editor. The hint text
// is used as query edit control's placeholder text and displayed when there is
// no user text in the control. The selection model and the text represents the
// text, cursor position and selected text in edit control.
class APP_LIST_MODEL_EXPORT SearchBoxModel {
 public:
  // The properties of the speech button.
  struct APP_LIST_MODEL_EXPORT SpeechButtonProperty {
    SpeechButtonProperty(const gfx::ImageSkia& icon,
                         const base::string16& tooltip,
                         const base::string16& accessible_name);
    ~SpeechButtonProperty();

    gfx::ImageSkia icon;
    base::string16 tooltip;

    // The accessibility name of the button.
    base::string16 accessible_name;
  };

  SearchBoxModel();
  ~SearchBoxModel();

  // Sets/gets the properties for the button of speech recognition.
  void SetSpeechRecognitionButton(
      std::unique_ptr<SpeechButtonProperty> speech_button);
  const SpeechButtonProperty* speech_button() const {
    return speech_button_.get();
  }

  // Sets/gets the hint text to display when there is in input.
  void SetHintText(const base::string16& hint_text);
  const base::string16& hint_text() const { return hint_text_; }

  // Sets the text for screen readers on the search box, and updates the
  // |accessible_name_|.
  void SetTabletAndClamshellAccessibleName(
      base::string16 tablet_accessible_name,
      base::string16 clamshell_accessible_name);

  // Changes the accessible name to clamshell or tablet friendly based on tablet
  // mode.
  void UpdateAccessibleName();
  const base::string16& accessible_name() const { return accessible_name_; }

  // Sets/gets the selection model for the search box's Textfield.
  void SetSelectionModel(const gfx::SelectionModel& sel);
  const gfx::SelectionModel& selection_model() const {
    return selection_model_;
  }

  void SetTabletMode(bool started);

  // Sets/gets the text for the search box's Textfield and the voice search
  // flag.
  void Update(const base::string16& text, bool is_voice_query);
  const base::string16& text() const { return text_; }
  bool is_voice_query() const { return is_voice_query_; }

  void AddObserver(SearchBoxModelObserver* observer);
  void RemoveObserver(SearchBoxModelObserver* observer);

 private:
  std::unique_ptr<SpeechButtonProperty> speech_button_;
  base::string16 hint_text_;
  base::string16 tablet_accessible_name_;
  base::string16 clamshell_accessible_name_;
  base::string16 accessible_name_;
  gfx::SelectionModel selection_model_;
  base::string16 text_;
  bool is_voice_query_;
  bool is_tablet_mode_;

  base::ObserverList<SearchBoxModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxModel);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_MODEL_SEARCH_BOX_MODEL_H_
