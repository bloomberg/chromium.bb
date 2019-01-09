// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/resources_private/resources_private_api.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/extensions/api/resources_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "pdf/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif

// To add a new component to this API, simply:
// 1. Add your component to the Component enum in
//      chrome/common/extensions/api/resources_private.idl
// 2. Create an AddStringsForMyComponent(base::DictionaryValue * dict) method.
// 3. Tie in that method to the switch statement in Run()

namespace extensions {

namespace {

void SetL10nString(base::DictionaryValue* dict, const std::string& string_id,
                   int resource_id) {
  dict->SetString(string_id, l10n_util::GetStringUTF16(resource_id));
}

void AddStringsForIdentity(base::DictionaryValue* dict) {
  SetL10nString(dict, "window-title", IDS_EXTENSION_CONFIRM_PERMISSIONS);
}

void AddStringsForPdf(base::DictionaryValue* dict) {
  SetL10nString(dict, "passwordDialogTitle", IDS_PDF_PASSWORD_DIALOG_TITLE);
  SetL10nString(dict, "passwordPrompt", IDS_PDF_NEED_PASSWORD);
  SetL10nString(dict, "passwordSubmit", IDS_PDF_PASSWORD_SUBMIT);
  SetL10nString(dict, "passwordInvalid", IDS_PDF_PASSWORD_INVALID);
  SetL10nString(dict, "pageLoading", IDS_PDF_PAGE_LOADING);
  SetL10nString(dict, "pageLoadFailed", IDS_PDF_PAGE_LOAD_FAILED);
  SetL10nString(dict, "errorDialogTitle", IDS_PDF_ERROR_DIALOG_TITLE);
  SetL10nString(dict, "pageReload", IDS_PDF_PAGE_RELOAD_BUTTON);
  SetL10nString(dict, "bookmarks", IDS_PDF_BOOKMARKS);
  SetL10nString(dict, "labelPageNumber", IDS_PDF_LABEL_PAGE_NUMBER);
  SetL10nString(dict, "tooltipRotateCW", IDS_PDF_TOOLTIP_ROTATE_CW);
  SetL10nString(dict, "tooltipDownload", IDS_PDF_TOOLTIP_DOWNLOAD);
  SetL10nString(dict, "tooltipPrint", IDS_PDF_TOOLTIP_PRINT);
  SetL10nString(dict, "tooltipFitToPage", IDS_PDF_TOOLTIP_FIT_PAGE);
  SetL10nString(dict, "tooltipFitToWidth", IDS_PDF_TOOLTIP_FIT_WIDTH);
  SetL10nString(dict, "tooltipZoomIn", IDS_PDF_TOOLTIP_ZOOM_IN);
  SetL10nString(dict, "tooltipZoomOut", IDS_PDF_TOOLTIP_ZOOM_OUT);
#if defined(OS_CHROMEOS)
  SetL10nString(dict, "tooltipAnnotate", IDS_PDF_ANNOTATION_ANNOTATE);
  SetL10nString(dict, "annotationDocumentTooLarge",
                IDS_PDF_ANNOTATION_DOCUMENT_TOO_LARGE);
  SetL10nString(dict, "annotationDocumentProtected",
                IDS_PDF_ANNOTATION_DOCUMENT_PROTECTED);
  SetL10nString(dict, "annotationDocumentRotated",
                IDS_PDF_ANNOTATION_DOCUMENT_ROTATED);
  SetL10nString(dict, "annotationPen", IDS_PDF_ANNOTATION_PEN);
  SetL10nString(dict, "annotationHighlighter", IDS_PDF_ANNOTATION_HIGHLIGHTER);
  SetL10nString(dict, "annotationEraser", IDS_PDF_ANNOTATION_ERASER);
  SetL10nString(dict, "annotationColorBlack", IDS_PDF_ANNOTATION_COLOR_BLACK);
  SetL10nString(dict, "annotationColorRed", IDS_PDF_ANNOTATION_COLOR_RED);
  SetL10nString(dict, "annotationColorYellow", IDS_PDF_ANNOTATION_COLOR_YELLOW);
  SetL10nString(dict, "annotationColorGreen", IDS_PDF_ANNOTATION_COLOR_GREEN);
  SetL10nString(dict, "annotationColorCyan", IDS_PDF_ANNOTATION_COLOR_CYAN);
  SetL10nString(dict, "annotationColorPurple", IDS_PDF_ANNOTATION_COLOR_PURPLE);
  SetL10nString(dict, "annotationColorBrown", IDS_PDF_ANNOTATION_COLOR_BROWN);
  SetL10nString(dict, "annotationColorWhite", IDS_PDF_ANNOTATION_COLOR_WHITE);
  SetL10nString(dict, "annotationColorCrimson",
                IDS_PDF_ANNOTATION_COLOR_CRIMSON);
  SetL10nString(dict, "annotationColorAmber", IDS_PDF_ANNOTATION_COLOR_AMBER);
  SetL10nString(dict, "annotationColorAvocadoGreen",
                IDS_PDF_ANNOTATION_COLOR_AVOCADO_GREEN);
  SetL10nString(dict, "annotationColorCobaltBlue",
                IDS_PDF_ANNOTATION_COLOR_COBALT_BLUE);
  SetL10nString(dict, "annotationColorDeepPurple",
                IDS_PDF_ANNOTATION_COLOR_DEEP_PURPLE);
  SetL10nString(dict, "annotationColorDarkBrown",
                IDS_PDF_ANNOTATION_COLOR_DARK_BROWN);
  SetL10nString(dict, "annotationColorDarkGrey",
                IDS_PDF_ANNOTATION_COLOR_DARK_GREY);
  SetL10nString(dict, "annotationColorHotPink",
                IDS_PDF_ANNOTATION_COLOR_HOT_PINK);
  SetL10nString(dict, "annotationColorOrange", IDS_PDF_ANNOTATION_COLOR_ORANGE);
  SetL10nString(dict, "annotationColorLime", IDS_PDF_ANNOTATION_COLOR_LIME);
  SetL10nString(dict, "annotationColorBlue", IDS_PDF_ANNOTATION_COLOR_BLUE);
  SetL10nString(dict, "annotationColorViolet", IDS_PDF_ANNOTATION_COLOR_VIOLET);
  SetL10nString(dict, "annotationColorTeal", IDS_PDF_ANNOTATION_COLOR_TEAL);
  SetL10nString(dict, "annotationColorLightGrey",
                IDS_PDF_ANNOTATION_COLOR_LIGHT_GREY);
  SetL10nString(dict, "annotationColorLightPink",
                IDS_PDF_ANNOTATION_COLOR_LIGHT_PINK);
  SetL10nString(dict, "annotationColorLightOrange",
                IDS_PDF_ANNOTATION_COLOR_LIGHT_ORANGE);
  SetL10nString(dict, "annotationColorLightGreen",
                IDS_PDF_ANNOTATION_COLOR_LIGHT_GREEN);
  SetL10nString(dict, "annotationColorLightBlue",
                IDS_PDF_ANNOTATION_COLOR_LIGHT_BLUE);
  SetL10nString(dict, "annotationColorLavender",
                IDS_PDF_ANNOTATION_COLOR_LAVENDER);
  SetL10nString(dict, "annotationColorLightTeal",
                IDS_PDF_ANNOTATION_COLOR_LIGHT_TEAL);
  SetL10nString(dict, "annotationSize1", IDS_PDF_ANNOTATION_SIZE1);
  SetL10nString(dict, "annotationSize2", IDS_PDF_ANNOTATION_SIZE2);
  SetL10nString(dict, "annotationSize3", IDS_PDF_ANNOTATION_SIZE3);
  SetL10nString(dict, "annotationSize4", IDS_PDF_ANNOTATION_SIZE4);
  SetL10nString(dict, "annotationSize8", IDS_PDF_ANNOTATION_SIZE8);
  SetL10nString(dict, "annotationSize12", IDS_PDF_ANNOTATION_SIZE12);
  SetL10nString(dict, "annotationSize16", IDS_PDF_ANNOTATION_SIZE16);
  SetL10nString(dict, "annotationSize20", IDS_PDF_ANNOTATION_SIZE20);
#endif
}

void AddAdditionalDataForPdf(base::DictionaryValue* dict) {
#if BUILDFLAG(ENABLE_PDF)
  dict->SetKey("pdfFormSaveEnabled",
               base::Value(base::FeatureList::IsEnabled(
                   chrome_pdf::features::kSaveEditedPDFForm)));
  dict->SetKey("pdfAnnotationsEnabled",
               base::Value(base::FeatureList::IsEnabled(
                   chrome_pdf::features::kPDFAnnotations)));
#endif
}

}  // namespace

namespace get_strings = api::resources_private::GetStrings;

ResourcesPrivateGetStringsFunction::ResourcesPrivateGetStringsFunction() {}

ResourcesPrivateGetStringsFunction::~ResourcesPrivateGetStringsFunction() {}

ExtensionFunction::ResponseAction ResourcesPrivateGetStringsFunction::Run() {
  std::unique_ptr<get_strings::Params> params(
      get_strings::Params::Create(*args_));
  auto dict = std::make_unique<base::DictionaryValue>();

  api::resources_private::Component component = params->component;

  switch (component) {
    case api::resources_private::COMPONENT_IDENTITY:
      AddStringsForIdentity(dict.get());
      break;
    case api::resources_private::COMPONENT_PDF:
      AddStringsForPdf(dict.get());
      AddAdditionalDataForPdf(dict.get());
      break;
    case api::resources_private::COMPONENT_NONE:
      NOTREACHED();
  }

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, dict.get());

  return RespondNow(OneArgument(std::move(dict)));
}

}  // namespace extensions
