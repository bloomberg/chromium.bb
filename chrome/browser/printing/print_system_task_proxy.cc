// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_system_task_proxy.h"

#include <ctype.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview_handler.h"
#include "printing/backend/print_backend.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"

#if defined(USE_CUPS)
#include <cups/cups.h>
#include <cups/ppd.h>

#include "base/file_util.h"
#endif

#if defined(USE_CUPS) && !defined(OS_MACOSX)
namespace printing_internal {

void parse_lpoptions(const FilePath& filepath, const std::string& printer_name,
                     int* num_options, cups_option_t** options) {
  std::string content;
  if (!file_util::ReadFileToString(filepath, &content))
    return;

  const char kDest[] = "dest";
  const char kDefault[] = "default";
  size_t kDestLen = sizeof(kDest) - 1;
  size_t kDefaultLen = sizeof(kDefault) - 1;
  std::vector <std::string> lines;
  base::SplitString(content, '\n', &lines);

  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line = lines[i];
    if (line.empty())
      continue;

    if (base::strncasecmp (line.c_str(), kDefault, kDefaultLen) == 0 &&
        isspace(line[kDefaultLen])) {
      line = line.substr(kDefaultLen);
    } else if (base::strncasecmp (line.c_str(), kDest, kDestLen) == 0 &&
               isspace(line[kDestLen])) {
      line = line.substr(kDestLen);
    } else {
      continue;
    }

    TrimWhitespaceASCII(line, TRIM_ALL, &line);
    if (line.empty())
      continue;

    size_t space_found = line.find(' ');
    if (space_found == std::string::npos)
      continue;

    std::string name = line.substr(0, space_found);
    if (name.empty())
      continue;

    if (base::strncasecmp(printer_name.c_str(), name.c_str(),
                          name.length()) != 0) {
      continue;  // This is not the required printer.
    }

    line = line.substr(space_found + 1);
    TrimWhitespaceASCII(line, TRIM_ALL, &line);  // Remove extra spaces.
    if (line.empty())
      continue;
    // Parse the selected printer custom options.
    *num_options = cupsParseOptions(line.c_str(), 0, options);
  }
}

void mark_lpoptions(const std::string& printer_name, ppd_file_t** ppd) {
  cups_option_t* options = NULL;
  int num_options = 0;
  ppdMarkDefaults(*ppd);

  const char kSystemLpOptionPath[] = "/etc/cups/lpoptions";
  const char kUserLpOptionPath[] = ".cups/lpoptions";

  std::vector<FilePath> file_locations;
  file_locations.push_back(FilePath(kSystemLpOptionPath));
  file_locations.push_back(FilePath(
      file_util::GetHomeDir().Append(kUserLpOptionPath)));

  for (std::vector<FilePath>::const_iterator it = file_locations.begin();
       it != file_locations.end(); ++it) {
    num_options = 0;
    options = NULL;
    parse_lpoptions(*it, printer_name, &num_options, &options);
    if (num_options > 0 && options) {
      cupsMarkOptions(*ppd, num_options, options);
      cupsFreeOptions(num_options, options);
    }
  }
}

}  // printing_internal namespace
#endif

using content::BrowserThread;

namespace {

const char kDisableColorOption[] = "disableColorOption";
const char kSetColorAsDefault[] = "setColorAsDefault";
const char kSetDuplexAsDefault[] = "setDuplexAsDefault";
const char kPrinterColorModelForBlack[] = "printerColorModelForBlack";
const char kPrinterColorModelForColor[] = "printerColorModelForColor";
const char kPrinterDefaultDuplexValue[] = "printerDefaultDuplexValue";

#if defined(OS_WIN)
const char kPskColor[] = "psk:Color";
const char kPskGray[] = "psk:Grayscale";
const char kPskMonochrome[] = "psk:Monochrome";
const char kPskDuplexFeature[] = "psk:JobDuplexAllDocumentsContiguously";
const char kPskTwoSided[] = "psk:TwoSided";
#elif defined(USE_CUPS)
const char kColorDevice[] = "ColorDevice";
const char kColorModel[] = "ColorModel";
const char kColorMode[] = "ColorMode";
const char kProcessColorModel[] = "ProcessColorModel";
const char kPrintoutMode[] = "PrintoutMode";
const char kDraftGray[] = "Draft.Gray";
const char kHighGray[] = "High.Gray";

const char kDuplex[] = "Duplex";
const char kDuplexNone[] = "None";

bool getBasicColorModelSettings(
    ppd_file_t* ppd, int* color_model_for_black, int* color_model_for_color,
    bool* color_is_default) {
  ppd_option_t* color_model = ppdFindOption(ppd, kColorModel);
  if (!color_model)
    return false;

  if (ppdFindChoice(color_model, printing::kBlack))
    *color_model_for_black = printing::BLACK;
  else if (ppdFindChoice(color_model, printing::kGray))
    *color_model_for_black = printing::GRAY;
  else if (ppdFindChoice(color_model, printing::kGrayscale))
    *color_model_for_black = printing::GRAYSCALE;

  if (ppdFindChoice(color_model, printing::kColor))
    *color_model_for_color = printing::COLOR;
  else if (ppdFindChoice(color_model, printing::kCMYK))
    *color_model_for_color = printing::CMYK;
  else if (ppdFindChoice(color_model, printing::kRGB))
    *color_model_for_color = printing::RGB;
  else if (ppdFindChoice(color_model, printing::kRGBA))
    *color_model_for_color = printing::RGBA;
  else if (ppdFindChoice(color_model, printing::kRGB16))
    *color_model_for_color = printing::RGB16;
  else if (ppdFindChoice(color_model, printing::kCMY))
    *color_model_for_color = printing::CMY;
  else if (ppdFindChoice(color_model, printing::kKCMY))
    *color_model_for_color = printing::KCMY;
  else if (ppdFindChoice(color_model, printing::kCMY_K))
    *color_model_for_color = printing::CMY_K;

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kColorModel);
  if (!marked_choice)
    marked_choice = ppdFindChoice(color_model, color_model->defchoice);

  if (marked_choice) {
    *color_is_default =
        (base::strcasecmp(marked_choice->choice, printing::kBlack) != 0) &&
        (base::strcasecmp(marked_choice->choice, printing::kGray) != 0);
  }
  return true;
}

bool getPrintOutModeColorSettings(
    ppd_file_t* ppd, int* color_model_for_black, int* color_model_for_color,
    bool* color_is_default) {
  ppd_option_t* printout_mode = ppdFindOption(ppd, kPrintoutMode);
  if (!printout_mode)
    return false;

  *color_model_for_color = printing::PRINTOUTMODE_NORMAL;
  *color_model_for_black = printing::PRINTOUTMODE_NORMAL;

  // Check to see if NORMAL_GRAY value is supported by PrintoutMode.
  // If NORMAL_GRAY is not supported, NORMAL value is used to
  // represent grayscale. If NORMAL_GRAY is supported, NORMAL is used to
  // represent color.
  if (ppdFindChoice(printout_mode, printing::kNormalGray))
    *color_model_for_black = printing::PRINTOUTMODE_NORMAL_GRAY;

  // Get the default marked choice to identify the default color setting
  // value.
  ppd_choice_t* printout_mode_choice = ppdFindMarkedChoice(ppd, kPrintoutMode);
  if (!printout_mode_choice) {
      printout_mode_choice = ppdFindChoice(printout_mode,
                                           printout_mode->defchoice);
  }
  if (printout_mode_choice) {
    if ((base::strcasecmp(printout_mode_choice->choice,
                          printing::kNormalGray) == 0) ||
        (base::strcasecmp(printout_mode_choice->choice, kHighGray) == 0) ||
        (base::strcasecmp(printout_mode_choice->choice, kDraftGray) == 0)) {
      *color_model_for_black = printing::PRINTOUTMODE_NORMAL_GRAY;
      *color_is_default = false;
    }
  }
  return true;
}

bool getColorModeSettings(
    ppd_file_t* ppd, int* color_model_for_black, int* color_model_for_color,
    bool* color_is_default) {
  // Samsung printers use "ColorMode" attribute in their ppds.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kColorMode);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, printing::kColor))
    *color_model_for_color = printing::COLORMODE_COLOR;

  if (ppdFindChoice(color_mode_option, printing::kMonochrome))
    *color_model_for_black = printing::COLORMODE_MONOCHROME;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kColorMode);
  if (!mode_choice) {
    mode_choice = ppdFindChoice(color_mode_option,
                                color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default =
        (base::strcasecmp(mode_choice->choice, printing::kColor) == 0);
  }
  return true;
}

bool getHPColorSettings(
    ppd_file_t* ppd, int* color_model_for_black, int* color_model_for_color,
    bool* color_is_default) {
  // HP printers use "Color/Color Model" attribute in their ppds.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, printing::kColor);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, printing::kColor))
    *color_model_for_color = printing::HP_COLOR_COLOR;
  if (ppdFindChoice(color_mode_option, printing::kBlack))
    *color_model_for_black = printing::HP_COLOR_BLACK;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kColorMode);
  if (!mode_choice) {
    mode_choice = ppdFindChoice(color_mode_option,
                                color_mode_option->defchoice);
  }
  if (mode_choice) {
    *color_is_default =
        (base::strcasecmp(mode_choice->choice, printing::kColor) == 0);
  }
  return true;
}

bool getProcessColorModelSettings(
    ppd_file_t* ppd, int* color_model_for_black, int* color_model_for_color,
    bool* color_is_default) {
  // Canon printers use "ProcessColorModel" attribute in their ppds.
  ppd_option_t* color_mode_option =  ppdFindOption(ppd, kProcessColorModel);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, printing::kRGB))
    *color_model_for_color = printing::PROCESSCOLORMODEL_RGB;
  else if (ppdFindChoice(color_mode_option, printing::kCMYK))
    *color_model_for_color = printing::PROCESSCOLORMODEL_CMYK;

  if (ppdFindChoice(color_mode_option, printing::kGreyscale))
    *color_model_for_black = printing::PROCESSCOLORMODEL_GREYSCALE;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kProcessColorModel);
  if (!mode_choice) {
    mode_choice = ppdFindChoice(color_mode_option,
                                color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default =
        (base::strcasecmp(mode_choice->choice, printing::kGreyscale) != 0);
  }
  return true;
}
#endif

}  // namespace

PrintSystemTaskProxy::PrintSystemTaskProxy(
    const base::WeakPtr<PrintPreviewHandler>& handler,
    printing::PrintBackend* print_backend,
    bool has_logged_printers_count)
    : handler_(handler),
      print_backend_(print_backend),
      has_logged_printers_count_(has_logged_printers_count) {
}

#if defined(UNIT_TEST) && defined(USE_CUPS)
// Only used for testing.
PrintSystemTaskProxy::PrintSystemTaskProxy() {
}
#endif

PrintSystemTaskProxy::~PrintSystemTaskProxy() {
}

void PrintSystemTaskProxy::GetDefaultPrinter() {
  VLOG(1) << "Get default printer start";
  std::string* default_printer = NULL;
  if (PrintPreviewHandler::last_used_printer_name_ == NULL) {
    default_printer = new std::string(print_backend_->GetDefaultPrinterName());
  } else {
    default_printer = new std::string(
        *PrintPreviewHandler::last_used_printer_name_);
  }
  VLOG(1) << "Get default printer finished, found: "
          << *default_printer;

  std::string* cloud_print_data = NULL;
  if (PrintPreviewHandler::last_used_printer_cloud_print_data_ != NULL) {
    cloud_print_data = new std::string(
        *PrintPreviewHandler::last_used_printer_cloud_print_data_);
  } else {
    cloud_print_data = new std::string;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrintSystemTaskProxy::SendDefaultPrinter, this,
                 default_printer, cloud_print_data));
}

void PrintSystemTaskProxy::SendDefaultPrinter(
    const std::string* default_printer, const std::string* cloud_print_data) {
  if (handler_)
    handler_->SendInitialSettings(*default_printer, *cloud_print_data);
  delete default_printer;
}

void PrintSystemTaskProxy::EnumeratePrinters() {
  VLOG(1) << "Enumerate printers start";
  ListValue* printers = new ListValue;
  printing::PrinterList printer_list;
  print_backend_->EnumeratePrinters(&printer_list);

  if (!has_logged_printers_count_) {
    // Record the total number of printers.
    UMA_HISTOGRAM_COUNTS("PrintPreview.NumberOfPrinters",
                         printer_list.size());
  }
  int i = 0;
  for (printing::PrinterList::iterator iter = printer_list.begin();
       iter != printer_list.end(); ++iter, ++i) {
    DictionaryValue* printer_info = new DictionaryValue;
    std::string printerName;
#if defined(OS_MACOSX)
    // On Mac, |iter->printer_description| specifies the printer name and
    // |iter->printer_name| specifies the device name / printer queue name.
    printerName = iter->printer_description;
#else
    printerName = iter->printer_name;
#endif
    printer_info->SetString(printing::kSettingPrinterName, printerName);
    printer_info->SetString(printing::kSettingDeviceName, iter->printer_name);
    VLOG(1) << "Found printer " << printerName
            << " with device name " << iter->printer_name;
    printers->Append(printer_info);
  }
  VLOG(1) << "Enumerate printers finished, found " << i << " printers";

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrintSystemTaskProxy::SetupPrinterList, this, printers));
}

void PrintSystemTaskProxy::SetupPrinterList(ListValue* printers) {
  if (handler_)
    handler_->SetupPrinterList(*printers);
  delete printers;
}

#if defined(USE_CUPS)
bool PrintSystemTaskProxy::GetPrinterCapabilitiesCUPS(
    const printing::PrinterCapsAndDefaults& printer_info,
    const std::string& printer_name,
    bool* set_color_as_default,
    int* printer_color_space_for_color,
    int* printer_color_space_for_black,
    bool* set_duplex_as_default,
    int* default_duplex_setting_value) {
  FilePath ppd_file_path;
  if (!file_util::CreateTemporaryFile(&ppd_file_path))
    return false;

  int data_size = printer_info.printer_capabilities.length();
  if (data_size != file_util::WriteFile(
                       ppd_file_path,
                       printer_info.printer_capabilities.data(),
                       data_size)) {
    file_util::Delete(ppd_file_path, false);
    return false;
  }

  ppd_file_t* ppd = ppdOpenFile(ppd_file_path.value().c_str());
  if (ppd) {
#if !defined(OS_MACOSX)
    printing_internal::mark_lpoptions(printer_name, &ppd);
#endif
    ppd_choice_t* duplex_choice = ppdFindMarkedChoice(ppd, kDuplex);
    if (!duplex_choice) {
      ppd_option_t* option = ppdFindOption(ppd, kDuplex);
      if (option)
        duplex_choice = ppdFindChoice(option, option->defchoice);
    }

    if (duplex_choice) {
      if (base::strcasecmp(duplex_choice->choice, kDuplexNone) != 0) {
        *set_duplex_as_default = true;
        *default_duplex_setting_value = printing::LONG_EDGE;
      } else {
        *default_duplex_setting_value = printing::SIMPLEX;
      }
    }

    bool is_color_device = false;
    ppd_attr_t* attr = ppdFindAttr(ppd, kColorDevice, NULL);
    if (attr && attr->value)
      is_color_device = ppd->color_device;
    *set_color_as_default = is_color_device;

    if (!((is_color_device && getBasicColorModelSettings(
              ppd, printer_color_space_for_black,
              printer_color_space_for_color, set_color_as_default)) ||
          getPrintOutModeColorSettings(
              ppd, printer_color_space_for_black,
              printer_color_space_for_color, set_color_as_default) ||
          getColorModeSettings(
              ppd, printer_color_space_for_black,
              printer_color_space_for_color, set_color_as_default) ||
          getHPColorSettings(
              ppd, printer_color_space_for_black,
              printer_color_space_for_color, set_color_as_default) ||
          getProcessColorModelSettings(
              ppd, printer_color_space_for_black,
              printer_color_space_for_color, set_color_as_default))) {
      VLOG(1) << "Unknown printer color model";
    }
    ppdClose(ppd);
  }
  file_util::Delete(ppd_file_path, false);
  return true;
}
#endif  // defined(USE_CUPS)

#if defined(OS_WIN)
void PrintSystemTaskProxy::GetPrinterCapabilitiesWin(
    const printing::PrinterCapsAndDefaults& printer_info,
    bool* set_color_as_default,
    int* printer_color_space_for_color,
    int* printer_color_space_for_black,
    bool* set_duplex_as_default,
    int* default_duplex_setting_value) {
  // According to XPS 1.0 spec, only color printers have psk:Color.
  // Therefore we don't need to parse the whole XML file, we just need to
  // search the string.  The spec can be found at:
  // http://msdn.microsoft.com/en-us/windows/hardware/gg463431.
  if (printer_info.printer_capabilities.find(kPskColor) != std::string::npos)
    *printer_color_space_for_color = printing::COLOR;

  if ((printer_info.printer_capabilities.find(kPskGray) !=
          std::string::npos) ||
      (printer_info.printer_capabilities.find(kPskMonochrome) !=
          std::string::npos)) {
    *printer_color_space_for_black = printing::GRAY;
  }
  *set_color_as_default =
      (printer_info.printer_defaults.find(kPskColor) != std::string::npos);

  *set_duplex_as_default =
      (printer_info.printer_defaults.find(kPskDuplexFeature) !=
          std::string::npos) &&
      (printer_info.printer_defaults.find(kPskTwoSided) !=
          std::string::npos);

  if (printer_info.printer_defaults.find(kPskDuplexFeature) !=
          std::string::npos) {
      if (printer_info.printer_defaults.find(kPskTwoSided) !=
              std::string::npos) {
        *default_duplex_setting_value = printing::LONG_EDGE;
      } else {
        *default_duplex_setting_value = printing::SIMPLEX;
    }
  }
}
#endif  // defined(OS_WIN)

void PrintSystemTaskProxy::GetPrinterCapabilities(
    const std::string& printer_name) {
  VLOG(1) << "Get printer capabilities start for " << printer_name;
  printing::PrinterCapsAndDefaults printer_info;
  if (!print_backend_->GetPrinterCapsAndDefaults(printer_name,
                                                 &printer_info)) {
    return;
  }

  bool set_color_as_default = false;
  bool set_duplex_as_default = false;
  int printer_color_space_for_color = printing::UNKNOWN_COLOR_MODEL;
  int printer_color_space_for_black = printing::UNKNOWN_COLOR_MODEL;
  int default_duplex_setting_value = printing::UNKNOWN_DUPLEX_MODE;

#if defined(USE_CUPS)
  if (!GetPrinterCapabilitiesCUPS(printer_info,
                                  printer_name,
                                  &set_color_as_default,
                                  &printer_color_space_for_color,
                                  &printer_color_space_for_black,
                                  &set_duplex_as_default,
                                  &default_duplex_setting_value)) {
    return;
  }
#elif defined(OS_WIN)
  GetPrinterCapabilitiesWin(printer_info,
                            &set_color_as_default,
                            &printer_color_space_for_color,
                            &printer_color_space_for_black,
                            &set_duplex_as_default,
                            &default_duplex_setting_value);
#else
  NOTIMPLEMENTED();
#endif
  bool disable_color_options = (!printer_color_space_for_color ||
                                !printer_color_space_for_black ||
                                (printer_color_space_for_color ==
                                 printer_color_space_for_black));

  DictionaryValue settings_info;
  settings_info.SetBoolean(kDisableColorOption, disable_color_options);
  if (printer_color_space_for_color == printing::UNKNOWN_COLOR_MODEL)
    printer_color_space_for_color = printing::COLOR;

  if (printer_color_space_for_black == printing::UNKNOWN_COLOR_MODEL)
    printer_color_space_for_black = printing::GRAY;

  if (disable_color_options ||
      PrintPreviewHandler::last_used_color_model_ ==
          printing::UNKNOWN_COLOR_MODEL) {
    settings_info.SetBoolean(kSetColorAsDefault, set_color_as_default);
  } else {
    settings_info.SetBoolean(kSetColorAsDefault,
                             printing::isColorModelSelected(
                                 PrintPreviewHandler::last_used_color_model_));
  }

  settings_info.SetBoolean(kSetDuplexAsDefault, set_duplex_as_default);
  settings_info.SetInteger(kPrinterColorModelForColor,
                           printer_color_space_for_color);
  settings_info.SetInteger(kPrinterColorModelForBlack,
                           printer_color_space_for_black);
  settings_info.SetInteger(kPrinterDefaultDuplexValue,
                           default_duplex_setting_value);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrintSystemTaskProxy::SendPrinterCapabilities, this,
                 settings_info.DeepCopy()));
}

void PrintSystemTaskProxy::SendPrinterCapabilities(
    DictionaryValue* settings_info) {
  if (handler_)
    handler_->SendPrinterCapabilities(*settings_info);
  delete settings_info;
}
