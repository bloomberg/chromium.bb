// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_UTIL_WIN_MOJOM_TRAITS_H_
#define CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_UTIL_WIN_MOJOM_TRAITS_H_

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "chrome/browser/conflicts/module_info_util_win.h"
#include "chrome/browser/conflicts/module_info_win.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "ui/shell_dialogs/execute_select_file_win.h"

namespace mojo {

template <>
struct EnumTraits<chrome::mojom::SelectFileDialogType,
                  ui::SelectFileDialog::Type> {
  static chrome::mojom::SelectFileDialogType ToMojom(
      ui::SelectFileDialog::Type input);
  static bool FromMojom(chrome::mojom::SelectFileDialogType input,
                        ui::SelectFileDialog::Type* output);
};

template <>
struct EnumTraits<chrome::mojom::CertificateType, CertificateInfo::Type> {
  static chrome::mojom::CertificateType ToMojom(CertificateInfo::Type input);
  static bool FromMojom(chrome::mojom::CertificateType input,
                        CertificateInfo::Type* output);
};

template <>
struct StructTraits<chrome::mojom::InspectionResultDataView,
                    ModuleInspectionResult> {
  static const base::string16& location(const ModuleInspectionResult& input);
  static const base::string16& basename(const ModuleInspectionResult& input);
  static const base::string16& product_name(
      const ModuleInspectionResult& input);
  static const base::string16& description(const ModuleInspectionResult& input);
  static const base::string16& version(const ModuleInspectionResult& input);
  static chrome::mojom::CertificateType certificate_type(
      const ModuleInspectionResult& input);
  static const base::FilePath& certificate_path(
      const ModuleInspectionResult& input);
  static const base::string16& certificate_subject(
      const ModuleInspectionResult& input);

  static bool Read(chrome::mojom::InspectionResultDataView data,
                   ModuleInspectionResult* output);
};

template <>
struct StructTraits<chrome::mojom::FileFilterSpecDataView, ui::FileFilterSpec> {
  static const base::string16& description(const ui::FileFilterSpec& input) {
    return input.description;
  }
  static const base::string16& extension_spec(const ui::FileFilterSpec& input) {
    return input.extension_spec;
  }

  static bool Read(chrome::mojom::FileFilterSpecDataView data,
                   ui::FileFilterSpec* output);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_UTIL_WIN_MOJOM_TRAITS_H_
