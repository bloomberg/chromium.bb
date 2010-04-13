// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/first_run_search_engine_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "gfx/font.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/google_chrome_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/standard_layout.h"
#include "views/view_text_utils.h"
#include "views/window/window.h"

namespace {

// These strings mark the embedded link in IDS_FIRSTRUN_SEARCH_SUBTEXT.
const wchar_t* kBeginLink = L"BEGIN_LINK_SE";
const wchar_t* kEndLink = L"END_LINK_SE";

// Represents an id for which we have no logo.
const int kNoLogo = -1;

// Size to scale logos down to if showing 4 instead of 3 choices. Logo images
// are all originally sized at 180 x 120 pixels, with the logo text baseline
// located 74 pixels beneath the top of the image.
const int kSmallLogoWidth = 120;
const int kSmallLogoHeight = 80;

// Used to pad text label height so it fits nicely in view.
const int kLabelPadding = 25;

// Maps the prepopulated id of a search engine to its logo.
struct SearchEngineLogo {
  // The unique id for a prepopulated search engine; see PrepopulatedEngine
  // struct in template_url_prepopulate_data.cc.
  int search_engine_id;

  // The IDR that represents the logo image for a given search engine.
  int logo_idr;
};

// Mapping of prepopulate_id to logo for each search engine.
SearchEngineLogo kSearchEngineLogos[] = {
  { 1, IDR_SEARCH_ENGINE_LOGO_GOOGLE },
  { 2, IDR_SEARCH_ENGINE_LOGO_YAHOO },
  { 3, IDR_SEARCH_ENGINE_LOGO_BING },
};

int GetSearchEngineLogo(const TemplateURL* template_url) {
  TemplateURL::IDType id = template_url->prepopulate_id();
  if (id > 0) {
    for (size_t i = 0; i < arraysize(kSearchEngineLogos); ++i) {
      if (kSearchEngineLogos[i].search_engine_id == id) {
        return kSearchEngineLogos[i].logo_idr;
      }
    }
  }
  // |id| is either not found, or does not exist.
  return kNoLogo;
}

}  // namespace

SearchEngineChoice::SearchEngineChoice(views::ButtonListener* listener,
                                       const TemplateURL* search_engine,
                                       bool use_small_logos)
    : NativeButton(listener, l10n_util::GetString(IDS_FR_SEARCH_CHOOSE)),
      is_image_label_(false),
      search_engine_(search_engine) {
  bool use_images = false;
#if defined(GOOGLE_CHROME_BUILD)
  use_images = true;
#endif
  int logo_id = GetSearchEngineLogo(search_engine_);
  if (use_images && logo_id != kNoLogo) {
    is_image_label_ = true;
    views::ImageView* logo_image = new views::ImageView();
    SkBitmap* logo_bmp =
        ResourceBundle::GetSharedInstance().GetBitmapNamed(logo_id);
    logo_image->SetImage(logo_bmp);
    if (use_small_logos)
      logo_image->SetImageSize(gfx::Size(kSmallLogoWidth, kSmallLogoHeight));
    // Tooltip text provides accessibility.
    logo_image->SetTooltipText(search_engine_->short_name());
    choice_view_.reset(logo_image);
  } else {
    // No logo -- we must show a text label.
    views::Label* logo_label = new views::Label(search_engine_->short_name());
    logo_label->SetColor(SK_ColorDKGRAY);
    logo_label->SetFont(logo_label->font().DeriveFont(2, gfx::Font::NORMAL));
    logo_label->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
    logo_label->SetTooltipText(search_engine_->short_name());
    choice_view_.reset(logo_label);
  }
}

int SearchEngineChoice::GetChoiceViewWidth() {
  return choice_view_->GetPreferredSize().width();
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
    SearchEngineViewObserver* observer, Profile* profile)
    : profile_(profile),
      observer_(observer),
      text_direction_is_rtl_(base::i18n::IsRTL()) {
  DCHECK(observer);
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
  observer_->SearchEngineChosen(
      static_cast<SearchEngineChoice*>(sender)->GetSearchEngine());
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
  // choices for the view:
  for (search_engine_iter = template_urls.begin();
       search_engine_iter < template_urls.begin() + 3;
       ++search_engine_iter) {
    // Push first three engines into buttons:
    SearchEngineChoice* choice = new SearchEngineChoice(this,
        *search_engine_iter, default_choice != NULL);
    search_engine_choices_.push_back(choice);
    AddChildView(choice->GetView());
    AddChildView(choice);
  }
  // Push the default choice to the fourth position.
  if (default_choice) {
    search_engine_choices_.push_back(default_choice);
    AddChildView(default_choice);
    AddChildView(default_choice->GetView());
  }

  // Now that we know how many logos to show, lay out and become visible.
  SetVisible(true);
  Layout();
  SchedulePaint();
}

void FirstRunSearchEngineView::OnKeywordEditorClosing(bool default_set) {
  // If the search engine has been set in the KeywordEditor, pass NULL
  // to the observer to show that we did not choose a search engine in this
  // dialog, and to notify that we're done with the selection process.
  if (default_set)
    observer_->SearchEngineChosen(NULL);
  // Else, the user cancelled the KeywordEditor, so continue with this dialog.
}

gfx::Size FirstRunSearchEngineView::GetPreferredSize() {
  return views::Window::GetLocalizedContentsSize(
      IDS_FIRSTRUN_SEARCH_ENGINE_SELECTION_WIDTH_CHARS,
      IDS_FIRSTRUN_SEARCH_ENGINE_SELECTION_HEIGHT_LINES);
}

void FirstRunSearchEngineView::LinkActivated(views::Link* source,
                                             int event_flags) {
  // The KeywordEditor is going to modify search_engines_model_, so
  // relinquish our observership so we don't try to redraw.
  search_engines_model_->RemoveObserver(this);
  // Launch search engine editing window from browser options dialog.
  KeywordEditorView::ShowAndObserve(profile_, this);
}

void FirstRunSearchEngineView::SetupControls() {
  using views::Background;
  using views::ImageView;
  using views::Label;
  using views::Link;
  using views::NativeButton;

  int label_width = GetPreferredSize().width() - 2 * kPanelHorizMargin;

  set_background(Background::CreateSolidBackground(SK_ColorWHITE));

  // Add title and text asking the user to choose a search engine:
  title_label_ = new Label(l10n_util::GetString(
      IDS_FR_SEARCH_MAIN_LABEL));
  title_label_->SetColor(SK_ColorBLACK);
  title_label_->SetFont(title_label_->font().DeriveFont(1, gfx::Font::BOLD));
  title_label_->SetMultiLine(true);
  title_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  title_label_->SizeToFit(label_width);
  AddChildView(title_label_);

  text_label_ = new Label(l10n_util::GetStringF(IDS_FR_SEARCH_TEXT,
      l10n_util::GetString(IDS_PRODUCT_NAME)));
  text_label_->SetColor(SK_ColorBLACK);
  text_label_->SetMultiLine(true);
  text_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  text_label_->SizeToFit(label_width);
  AddChildView(text_label_);

  // The first separator marks the start of the search engine choice panel.
  separator_1_ = new views::Separator;
  AddChildView(separator_1_);

  // The second separator marks the end of the search engine choice panel.
  separator_2_ = new views::Separator;
  AddChildView(separator_2_);

  // Parse out the link to the internal search engine options dialog.
  std::wstring subtext = l10n_util::GetStringF(IDS_FR_SEARCH_SUBTEXT,
      l10n_util::GetString(IDS_PRODUCT_NAME));

  size_t link_begin = subtext.find(kBeginLink);
  DCHECK(link_begin != std::wstring::npos);
  size_t link_end = subtext.find(kEndLink);
  DCHECK(link_end != std::wstring::npos);

  subtext_label_1_ = new Label(subtext.substr(0, link_begin));
  options_link_ = new views::Link(subtext.substr(link_begin +
      wcslen(kBeginLink), link_end - link_begin - wcslen(kBeginLink)));
  AddChildView(options_link_);
  options_link_->SetController(this);
  subtext_label_2_ = new Label(subtext.substr(link_end + wcslen(kEndLink)));

  // This label is never actually shown -- it's just used to place the subtext
  // strings correctly in the view.
  std::wstring dummy_label = subtext.substr(wcslen(kBeginLink) +
                                            wcslen(kEndLink) + 4);
  dummy_subtext_label_ = new Label(dummy_label);
  dummy_subtext_label_->SetMultiLine(true);
  dummy_subtext_label_->SizeToFit(label_width);
}

void FirstRunSearchEngineView::Layout() {
  // General vertical spacing between elements:
  const int kVertSpacing = 8;
  // Vertical spacing between the logo + button section and the separators:
  const int kUpperLogoMargin = 40;
  const int kLowerLogoMargin = 65;
  // Percentage of vertical space around logos to use for upper padding.
  const double kUpperPaddingPercent = 0.1;

  int num_choices = search_engine_choices_.size();

  // Title and text above top separator.
  int label_width = GetPreferredSize().width() - 2 * kPanelHorizMargin;
  int label_height = GetPreferredSize().height() - 2 * kPanelVertMargin;

  title_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
      label_width, title_label_->GetPreferredSize().height());

  int next_v_space = title_label_->y() +
                     title_label_->height() + kVertSpacing;

  text_label_->SetBounds(kPanelHorizMargin, next_v_space,
                         label_width,
                         text_label_->GetPreferredSize().height());
  next_v_space = text_label_->y() +
                 text_label_->height() + kVertSpacing;

  separator_1_->SetBounds(kPanelHorizMargin, next_v_space, label_width,
                          separator_1_->GetPreferredSize().height());

  // Set the logos and buttons between the upper and lower separators:
  if (num_choices > 0) {
    int logo_width = search_engine_choices_[0]->GetChoiceViewWidth();
    int logo_height = search_engine_choices_[0]->GetChoiceViewHeight();
    int button_width = search_engine_choices_[0]->GetPreferredSize().width();
    int button_height = search_engine_choices_[0]->GetPreferredSize().height();
    int lower_section_height = dummy_subtext_label_->height() + kVertSpacing +
                               separator_2_->GetPreferredSize().height();
    int logo_section_height = logo_height + kVertSpacing + button_height;
    int lower_section_start = label_height + kPanelVertMargin -
                              lower_section_height;
    int upper_logo_margin =
        static_cast<int>((lower_section_start - separator_1_->y() -
                          separator_1_->height() - logo_section_height) *
                         kUpperPaddingPercent);

    next_v_space = separator_1_->y() + separator_1_->height() +
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
    search_engine_choices_[2]->SetChoiceViewBounds(
        next_h_space, next_v_space, logo_width, logo_height);

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
    search_engine_choices_[2]->SetBounds(next_h_space, next_v_space,
                                         button_width, button_height);

    if (num_choices > 3) {
      next_h_space = search_engine_choices_[2]->x() + logo_width +
                     logo_padding;
      search_engine_choices_[3]->SetBounds(next_h_space, next_v_space,
                                           button_width, button_height);
    }

    // Lower separator and text beneath it.
    next_v_space = lower_section_start;
  }  // if (search_engine_choices.size() > 0)

  separator_2_->SetBounds(kPanelHorizMargin, next_v_space, label_width,
                          separator_2_->GetPreferredSize().height());

  next_v_space = separator_2_->y() + separator_2_->height() + kVertSpacing;

  // This label is used by view_text_utils::DrawTextAndPositionUrl in order
  // to figure out mirrored x positions for RTL languages.  It only needs to
  // provide the correct origin and width; height is not used.
  dummy_subtext_label_->SetBounds(kPanelHorizMargin, next_v_space,
                                 label_width, dummy_subtext_label_->height());
}

void FirstRunSearchEngineView::Paint(gfx::Canvas* canvas) {
  views::View::Paint(canvas);

  gfx::Rect link_rect;
  gfx::Size position;
  gfx::Font font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
  const gfx::Rect label_bounds = dummy_subtext_label_->bounds();

  view_text_utils::DrawTextAndPositionUrl(canvas, dummy_subtext_label_,
      subtext_label_1_->GetText(), options_link_, &link_rect, &position,
      text_direction_is_rtl_, label_bounds, font);
  view_text_utils::DrawTextAndPositionUrl(canvas, dummy_subtext_label_,
      subtext_label_2_->GetText(), NULL, NULL, &position,
      text_direction_is_rtl_, label_bounds, font);

  options_link_->SetBounds(link_rect.x(), link_rect.y(), link_rect.width(),
                           link_rect.height());
}

std::wstring FirstRunSearchEngineView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_FIRSTRUN_DLG_TITLE);
}

void FirstRunSearchEngineView::WindowClosing() {
  // User decided not to choose a search engine after all.
  observer_->SearchEngineChosen(NULL);
}

