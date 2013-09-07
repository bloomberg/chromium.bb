// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "url/gurl.h"

namespace autofill {

class AutofillField;

// The time (in milliseconds) to show the splash page when the dialog is first
// started.
extern const int kSplashDisplayDurationMs;
// The time (in milliseconds) spend fading out the splash image.
extern const int kSplashFadeOutDurationMs;
// The time (in milliseconds) spend fading in the dialog (after the splash image
// has been faded out).
extern const int kSplashFadeInDialogDurationMs;

// This struct describes a single input control for the imperative autocomplete
// dialog.
struct DetailInput {
  // Multiple DetailInput structs with the same row_id go on the same row. The
  // actual order of the rows is determined by their order of appearance in
  // kBillingInputs. If negative, don't show the input at all (leave it hidden
  // at all times).
  int row_id;
  ServerFieldType type;
  // Placeholder text resource ID.
  int placeholder_text_rid;
  // A number between 0 and 1.0 that describes how much of the horizontal space
  // in the row should be allotted to this input. 0 is equivalent to 1.
  float expand_weight;
  // When non-empty, indicates the starting value for this input. This will be
  // used when the user is editing existing data.
  string16 initial_value;
  // Whether the input is able to be edited (e.g. text changed in textfields,
  // index changed in comboboxes).
  bool editable;
};

// Determines whether |input| and |field| match.
typedef base::Callback<bool(const DetailInput& input,
                            const AutofillField& field)>
    InputFieldComparator;

// Sections of the dialog --- all fields that may be shown to the user fit under
// one of these sections.
enum DialogSection {
  // Lower boundary value for looping over all sections.
  SECTION_MIN,

  // The Autofill-backed dialog uses separate CC and billing sections.
  SECTION_CC = SECTION_MIN,
  SECTION_BILLING,
  // The wallet-backed dialog uses a combined CC and billing section.
  SECTION_CC_BILLING,
  SECTION_SHIPPING,

  // Upper boundary value for looping over all sections.
  SECTION_MAX = SECTION_SHIPPING
};

// A notification to show in the autofill dialog. Ranges from information to
// seriously scary security messages, and will give you the color it should be
// displayed (if you ask it).
class DialogNotification {
 public:
  enum Type {
    NONE,
    DEVELOPER_WARNING,
    EXPLANATORY_MESSAGE,
    REQUIRED_ACTION,
    SECURITY_WARNING,
    VALIDATION_ERROR,
    WALLET_ERROR,
    WALLET_USAGE_CONFIRMATION,
  };

  DialogNotification();
  DialogNotification(Type type, const string16& display_text);
  ~DialogNotification();

  // Returns the appropriate background, border, or text color for the view's
  // notification area based on |type_|.
  SkColor GetBackgroundColor() const;
  SkColor GetBorderColor() const;
  SkColor GetTextColor() const;

  // Whether this notification has an arrow pointing up at the account chooser.
  bool HasArrow() const;

  // Whether this notifications has the "Save details to wallet" checkbox.
  bool HasCheckbox() const;

  Type type() const { return type_; }
  const string16& display_text() const { return display_text_; }

  void set_link_url(const GURL& link_url) { link_url_ = link_url; }
  const GURL& link_url() const { return link_url_; }

  const gfx::Range& link_range() const { return link_range_; }

  void set_tooltip_text(const string16& tooltip_text) {
    tooltip_text_ = tooltip_text;
  }
  const string16& tooltip_text() const { return tooltip_text_; }

  void set_checked(bool checked) { checked_ = checked; }
  bool checked() const { return checked_; }

  void set_interactive(bool interactive) { interactive_ = interactive; }
  bool interactive() const { return interactive_; }

 private:
  Type type_;
  string16 display_text_;

  // If the notification includes a link, these describe the destination and
  // which part of |display_text_| is the anchor text.
  GURL link_url_;
  gfx::Range link_range_;

  // When non-empty, indicates that a tooltip should be shown on the end of
  // the notification.
  string16 tooltip_text_;

  // Whether the dialog notification's checkbox should be checked. Only applies
  // when |HasCheckbox()| is true.
  bool checked_;

  // When false, this disables user interaction with the notification. For
  // example, WALLET_USAGE_CONFIRMATION notifications set this to false after
  // the submit flow has started.
  bool interactive_;
};

extern SkColor const kWarningColor;

struct SuggestionState {
  SuggestionState();
  SuggestionState(bool visible,
                  const string16& vertically_compact_text,
                  const string16& horizontally_compact_text,
                  const gfx::Image& icon,
                  const string16& extra_text,
                  const gfx::Image& extra_icon);
  ~SuggestionState();
  // Whether a suggestion should be shown.
  bool visible;
  // Text to be shown for the suggestion. This should be preferred over
  // |horizontally_compact_text| when there's enough horizontal space available
  // to display it. When there's not enough space, fall back to
  // |horizontally_compact_text|.
  base::string16 vertically_compact_text;
  base::string16 horizontally_compact_text;
  gfx::Image icon;
  string16 extra_text;
  gfx::Image extra_icon;
};

// A struct to describe a textual message within a dialog overlay.
struct DialogOverlayString {
  DialogOverlayString();
  ~DialogOverlayString();
  base::string16 text;
  // TODO(estade): should be able to remove this; text is always black.
  SkColor text_color;
  gfx::Font font;
  // TODO(estade): should be able to remove this; text is always centered.
  gfx::HorizontalAlignment alignment;
};

// A struct to describe a dialog overlay. If |image| is empty, no overlay should
// be shown.
struct DialogOverlayState {
  DialogOverlayState();
  ~DialogOverlayState();
  // If empty, there should not be an overlay. If non-empty, an image that is
  // more or less front and center.
  gfx::Image image;
  // If non-empty, messages to display.
  // TODO(estade): make this a single string, no longer need to support multiple
  // messages.
  std::vector<DialogOverlayString> strings;
  // The amount of time this dialog is valid for. After this time has elapsed,
  // the view should update the overlay.
  base::TimeDelta expiry;
};

enum ValidationType {
  VALIDATE_EDIT,   // Validate user edits. Allow for empty fields.
  VALIDATE_FINAL,  // Full form validation. Required fields can't be empty.
};

typedef std::vector<DetailInput> DetailInputs;
typedef std::map<const DetailInput*, string16> DetailOutputMap;

typedef std::map<ServerFieldType, string16> ValidityData;

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_
