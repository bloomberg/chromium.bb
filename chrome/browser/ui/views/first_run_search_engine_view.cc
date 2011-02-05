// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_search_engine_view.h"

#include <algorithm>
#include <map>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/rand_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/options/options_window.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/google_chrome_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "views/controls/button/button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/layout/layout_constants.h"
#include "views/view_text_utils.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

using base::Time;

namespace {

// Size to scale logos down to if showing 4 instead of 3 choices. Logo images
// are all originally sized at 180 x 120 pixels, with the logo text baseline
// located 74 pixels beneath the top of the image.
const int kSmallLogoWidth = 132;
const int kSmallLogoHeight = 88;

// Used to pad text label height so it fits nicely in view.
const int kLabelPadding = 25;

}  // namespace

SearchEngineChoice::SearchEngineChoice(views::ButtonListener* listener,
                                       const TemplateURL* search_engine,
                                       bool use_small_logos)
    : NativeButton(
          listener,
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_FR_SEARCH_CHOOSE))),
      is_image_label_(false),
      search_engine_(search_engine),
      slot_(0) {
  bool use_images = false;
#if defined(GOOGLE_CHROME_BUILD)
  use_images = true;
#endif
  int logo_id = search_engine_->logo_id();
  if (use_images && logo_id != kNoSearchEngineLogo) {
    is_image_label_ = true;
    views::ImageView* logo_image = new views::ImageView();
    SkBitmap* logo_bmp =
        ResourceBundle::GetSharedInstance().GetBitmapNamed(logo_id);
    logo_image->SetImage(logo_bmp);
    if (use_small_logos)
      logo_image->SetImageSize(gfx::Size(kSmallLogoWidth, kSmallLogoHeight));
    // Tooltip text provides accessibility for low-vision users.
    logo_image->SetTooltipText(search_engine_->short_name());
    choice_view_ = logo_image;
  } else {
    // No logo -- we must show a text label.
    views::Label* logo_label = new views::Label(search_engine_->short_name());
    logo_label->SetColor(SK_ColorDKGRAY);
    logo_label->SetFont(logo_label->font().DeriveFont(3, gfx::Font::BOLD));
    logo_label->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
    logo_label->SetTooltipText(search_engine_->short_name());
    logo_label->SetMultiLine(true);
    logo_label->SizeToFit(kSmallLogoWidth);
    choice_view_ = logo_label;
  }

  // The accessible name of the button provides accessibility for
  // screenreaders. It uses the browser name rather than the text of the
  // button "Choose", since it's not obvious to a screenreader user which
  // browser each button corresponds to.
  SetAccessibleName(WideToUTF16Hack(search_engine_->short_name()));
}

int SearchEngineChoice::GetChoiceViewWidth() {
  if (is_image_label_)
    return choice_view_->GetPreferredSize().width();
  else
    return kSmallLogoWidth;
}

int SearchEngineChoice::GetChoiceViewHeight() {
  if (!is_image_label_) {
    // Labels need to be padded to look nicer.
    return choice_view_->GetPreferredSize().height() + kLabelPadding;
  } else {
    return choice_view_->GetPreferredSize().height();
  }
}

void SearchEngineChoice::SetChoiceViewBounds(int x, int y, int width,
                                             int height) {
  choice_view_->SetBounds(x, y, width, height);
}

FirstRunSearchEngineView::FirstRunSearchEngineView(
    Profile* profile, bool randomize)
    : background_image_(NULL),
      profile_(profile),
      text_direction_is_rtl_(base::i18n::IsRTL()),
      randomize_(randomize) {
  // Don't show ourselves until all the search engines have loaded from
  // the profile -- otherwise we have nothing to show.
  SetVisible(false);

  // Start loading the search engines for the given profile.
  search_engines_model_ = profile_->GetTemplateURLModel();
  if (search_engines_model_) {
    DCHECK(!search_engines_model_->loaded());
    search_engines_model_->AddObserver(this);
    search_engines_model_->Load();
  } else {
    NOTREACHED();
  }
  SetupControls();
}

FirstRunSearchEngineView::~FirstRunSearchEngineView() {
  search_engines_model_->RemoveObserver(this);
}

void FirstRunSearchEngineView::ButtonPressed(views::Button* sender,
                                             const views::Event& event) {
  SearchEngineChoice* choice = static_cast<SearchEngineChoice*>(sender);
  TemplateURLModel* template_url_model = profile_->GetTemplateURLModel();
  DCHECK(template_url_model);
  template_url_model->SetSearchEngineDialogSlot(choice->slot());
  const TemplateURL* default_search = choice->GetSearchEngine();
  if (default_search)
    template_url_model->SetDefaultSearchProvider(default_search);

  MessageLoop::current()->Quit();
}

void FirstRunSearchEngineView::Paint(gfx::Canvas* canvas) {
  // Fill in behind the background image with the standard gray toolbar color.
  canvas->FillRectInt(SkColorSetRGB(237, 238, 237), 0, 0, width(),
                      background_image_->height());
  // The rest of the dialog background should be white.
  DCHECK(height() > background_image_->height());
  canvas->FillRectInt(SK_ColorWHITE, 0, background_image_->height(), width(),
                      height() - background_image_->height());
}

void FirstRunSearchEngineView::OnTemplateURLModelChanged() {
  using views::ImageView;

  // We only watch the search engine model change once, on load.  Remove
  // observer so we don't try to redraw if engines change under us.
  search_engines_model_->RemoveObserver(this);

  // Add search engines in search_engines_model_ to buttons list.  The
  // first three will always be from prepopulated data.
  std::vector<const TemplateURL*> template_urls =
      search_engines_model_->GetTemplateURLs();

  // If we have fewer than two search engines, end search engine dialog
  // immediately, leaving imported default search engine setting intact.
  if (template_urls.size() < 2) {
    MessageLoop::current()->Quit();
    return;
  }

  std::vector<const TemplateURL*>::iterator search_engine_iter;

  // Is user's default search engine included in first three prepopulated
  // set?  If not, we need to expand the dialog to include a fourth engine.
  const TemplateURL* default_search_engine =
      search_engines_model_->GetDefaultSearchProvider();
  // If the user's default choice is not in the first three search engines
  // in template_urls, store it in |default_choice| and provide it as a
  // fourth option.
  SearchEngineChoice* default_choice = NULL;

  // First, see if we have 4 logos to show (in which case we use small logos).
  // We show 4 logos when the default search engine the user has chosen is
  // not one of the first three prepopulated engines.
  if (template_urls.size() > 3) {
    for (search_engine_iter = template_urls.begin() + 3;
         search_engine_iter != template_urls.end();
         ++search_engine_iter) {
      if (default_search_engine == *search_engine_iter) {
        default_choice = new SearchEngineChoice(this, *search_engine_iter,
                                                true);
      }
    }
  }

  // Now that we know what size the logos should be, create new search engine
  // choices for the view.  If there are 2 search engines, only show 2
  // choices; for 3 or more, show 3 (unless the default is not one of the
  // top 3, in which case show 4).
  for (search_engine_iter = template_urls.begin();
       search_engine_iter < template_urls.begin() +
           (template_urls.size() < 3 ? 2 : 3);
       ++search_engine_iter) {
    // Push first three engines into buttons:
    SearchEngineChoice* choice = new SearchEngineChoice(this,
        *search_engine_iter, default_choice != NULL);
    search_engine_choices_.push_back(choice);
    AddChildView(choice->GetView());  // The logo or text view.
    AddChildView(choice);  // The button associated with the choice.
  }
  // Push the default choice to the fourth position.
  if (default_choice) {
    search_engine_choices_.push_back(default_choice);
    AddChildView(default_choice->GetView());  // The logo or text view.
    AddChildView(default_choice);  // The button associated with the choice.
  }

  // Randomize order of logos if option has been set.
  if (randomize_) {
    std::random_shuffle(search_engine_choices_.begin(),
                        search_engine_choices_.end(),
                        base::RandGenerator);
    // Assign to each choice the position in which it is shown on the screen.
    std::vector<SearchEngineChoice*>::iterator it;
    int slot = 0;
    for (it = search_engine_choices_.begin();
         it != search_engine_choices_.end();
         it++) {
      (*it)->set_slot(slot++);
    }
  }

  // Now that we know how many logos to show, lay out and become visible.
  SetVisible(true);
  Layout();
  SchedulePaint();

  // If the widget has detected that a screenreader is running, change the
  // button names from "Choose" to the name of the search engine. This works
  // around a bug that JAWS ignores the accessible name of a native button.
  if (GetWidget() && GetWidget()->IsAccessibleWidget()) {
    std::vector<SearchEngineChoice*>::iterator it;
    for (it = search_engine_choices_.begin();
         it != search_engine_choices_.end();
         it++) {
      (*it)->SetLabel((*it)->GetSearchEngine()->short_name());
    }
  }

  // This will tell screenreaders that they should read the full text
  // of this dialog to the user now (rather than waiting for the user
  // to explore it).
  NotifyAccessibilityEvent(AccessibilityTypes::EVENT_ALERT);
}

gfx::Size FirstRunSearchEngineView::GetPreferredSize() {
  return views::Window::GetLocalizedContentsSize(
      IDS_FIRSTRUN_SEARCH_ENGINE_SELECTION_WIDTH_CHARS,
      IDS_FIRSTRUN_SEARCH_ENGINE_SELECTION_HEIGHT_LINES);
}

void FirstRunSearchEngineView::SetupControls() {
  using views::Background;
  using views::ImageView;
  using views::Label;
  using views::NativeButton;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  background_image_ = new views::ImageView();
  background_image_->SetImage(rb.GetBitmapNamed(IDR_SEARCH_ENGINE_DIALOG_TOP));
  background_image_->EnableCanvasFlippingForRTLUI(true);
  if (text_direction_is_rtl_) {
    background_image_->SetHorizontalAlignment(ImageView::LEADING);
  } else {
    background_image_->SetHorizontalAlignment(ImageView::TRAILING);
  }

  AddChildView(background_image_);

  int label_width = GetPreferredSize().width() - 2 * kPanelHorizMargin;

  // Add title and text asking the user to choose a search engine:
  title_label_ = new Label(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_FR_SEARCH_MAIN_LABEL)));
  title_label_->SetColor(SK_ColorBLACK);
  title_label_->SetFont(title_label_->font().DeriveFont(3, gfx::Font::BOLD));
  title_label_->SetMultiLine(true);
  title_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  title_label_->SizeToFit(label_width);
  AddChildView(title_label_);

  text_label_ = new Label(UTF16ToWide(l10n_util::GetStringFUTF16(
      IDS_FR_SEARCH_TEXT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME))));
  text_label_->SetColor(SK_ColorBLACK);
  text_label_->SetFont(text_label_->font().DeriveFont(1, gfx::Font::NORMAL));
  text_label_->SetMultiLine(true);
  text_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  text_label_->SizeToFit(label_width);
  AddChildView(text_label_);
}

void FirstRunSearchEngineView::Layout() {
  // Disable the close button.
  GetWindow()->EnableClose(false);

  gfx::Size pref_size = background_image_->GetPreferredSize();
  background_image_->SetBounds(0, 0, GetPreferredSize().width(),
                               pref_size.height());

  // General vertical spacing between elements:
  const int kVertSpacing = 8;
  // Percentage of vertical space around logos to use for upper padding.
  const double kUpperPaddingPercent = 0.4;

  int num_choices = search_engine_choices_.size();
  int label_width = GetPreferredSize().width() - 2 * kPanelHorizMargin;
  int label_height = GetPreferredSize().height() - 2 * kPanelVertMargin;

  // Set title.
  title_label_->SetBounds(kPanelHorizMargin,
      pref_size.height() / 2 - title_label_->GetPreferredSize().height() / 2,
      label_width, title_label_->GetPreferredSize().height());

  int next_v_space = background_image_->height() + kVertSpacing * 2;

  // Set text describing search engine hooked into omnibox.
  text_label_->SetBounds(kPanelHorizMargin, next_v_space,
                         label_width,
                         text_label_->GetPreferredSize().height());
  next_v_space = text_label_->y() +
                 text_label_->height() + kVertSpacing;

  // Logos and buttons
  if (num_choices > 0) {
    // All search engine logos are sized the same, so the size of the first is
    // generally valid as the size of all.
    int logo_width = search_engine_choices_[0]->GetChoiceViewWidth();
    int logo_height = search_engine_choices_[0]->GetChoiceViewHeight();
    int button_width = search_engine_choices_[0]->GetPreferredSize().width();
    int button_height = search_engine_choices_[0]->GetPreferredSize().height();

    int logo_section_height = logo_height + kVertSpacing + button_height;
    // Upper logo margin gives the amount of whitespace between the text label
    // and the logo field.  The total amount of whitespace available is equal
    // to the height of the whole label subtracting the heights of the logo
    // section itself, the top image, the text label, and vertical spacing
    // between those elements.
    int upper_logo_margin =
        static_cast<int>((label_height - logo_section_height -
            background_image_->height() - text_label_->height()
            - kVertSpacing + kPanelVertMargin) * kUpperPaddingPercent);

    next_v_space = text_label_->y() + text_label_->height() +
                   upper_logo_margin;

    // The search engine logos (which all have equal size):
    int logo_padding =
        (label_width - (num_choices * logo_width)) / (num_choices + 1);

    search_engine_choices_[0]->SetChoiceViewBounds(
        kPanelHorizMargin + logo_padding, next_v_space, logo_width,
        logo_height);

    int next_h_space = search_engine_choices_[0]->GetView()->x() +
                       logo_width + logo_padding;
    search_engine_choices_[1]->SetChoiceViewBounds(
        next_h_space, next_v_space, logo_width, logo_height);

    next_h_space = search_engine_choices_[1]->GetView()->x() + logo_width +
                   logo_padding;
    if (num_choices > 2) {
      search_engine_choices_[2]->SetChoiceViewBounds(
          next_h_space, next_v_space, logo_width, logo_height);
    }

    if (num_choices > 3) {
      next_h_space = search_engine_choices_[2]->GetView()->x() + logo_width +
                     logo_padding;
      search_engine_choices_[3]->SetChoiceViewBounds(
          next_h_space, next_v_space, logo_width, logo_height);
    }

    next_v_space = search_engine_choices_[0]->GetView()->y() + logo_height +
                   kVertSpacing;

    // The buttons for search engine selection:
    int button_padding = logo_padding + logo_width / 2 - button_width / 2;

    search_engine_choices_[0]->SetBounds(kPanelHorizMargin + button_padding,
                                         next_v_space, button_width,
                                         button_height);

    next_h_space = search_engine_choices_[0]->x() + logo_width + logo_padding;
    search_engine_choices_[1]->SetBounds(next_h_space, next_v_space,
                                         button_width, button_height);
    next_h_space = search_engine_choices_[1]->x() + logo_width + logo_padding;
    if (num_choices > 2) {
      search_engine_choices_[2]->SetBounds(next_h_space, next_v_space,
                                           button_width, button_height);
    }

    if (num_choices > 3) {
      next_h_space = search_engine_choices_[2]->x() + logo_width +
                     logo_padding;
      search_engine_choices_[3]->SetBounds(next_h_space, next_v_space,
                                           button_width, button_height);
    }
  }  // if (search_engine_choices.size() > 0)
}

AccessibilityTypes::Role FirstRunSearchEngineView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_ALERT;
}

std::wstring FirstRunSearchEngineView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_FIRSTRUN_DLG_TITLE));
}
