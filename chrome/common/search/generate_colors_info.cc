// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/search/selected_colors_info.h"

// TODO(gayane): Replace with real template.
// Template for the icon svg.
// $1 - primary color
// $2 - secondary color
const char kIconTemplate[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?><!DOCTYPE svg "
    "PUBLIC \"-//W3C//DTD SVG 1.1//EN\" "
    "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"><svg version=\"1.1\" "
    "xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
    "preserveAspectRatio=\"xMidYMid meet\" viewBox=\"0 0 640 640\" "
    "width=\"640\" height=\"640\"><defs><path d=\"M0 0L640 0L640 640L0 640L0 "
    "0Z\" id=\"a1Y0EmMoCp\"></path><path d=\"M499.23 317.62C499.23 396.82 "
    "420.79 461.12 324.17 461.12C227.55 461.12 149.11 396.82 149.11 "
    "317.62C149.11 238.42 227.55 174.12 324.17 174.12C420.79 174.12 499.23 "
    "238.42 499.23 317.62Z\" id=\"f7eUC88KqB\"></path></defs><g><g><g "
    "transform=\"matrix(1 0 0 1 0 0)\" "
    "vector-effect=\"non-scaling-stroke\"><use xlink:href=\"#a1Y0EmMoCp\" "
    "opacity=\"1\" fill=$1 fill-opacity=\"1\"></use></g><g><use "
    "xlink:href=\"#f7eUC88KqB\" opacity=\"1\" fill=$2 "
    "fill-opacity=\"1\"></use></g></g></g></svg>";

// Template for color info line.
// $1 - color id
// $2 - red value of primary color
// $3 - green value of primary color
// $4 - blue value of primary color
// $5 - color label
// $6 - icon data
const char kColorInfoLineTemplate[] =
    "   ColorInfo($1, SkColorSetRGB($2, $3, $4), \"$5\", "
    "\"data:image/svg+xml;base64,$6\")";

// Template for the generated file content.
// $1 - lines for updated color info.
// $2 - number of colors.
const char kFileContentTemplate[] =
    "// Generated from generate_colors_info.cc. Do not edit!\n"
    "\n"
    "#ifndef CHROME_COMMON_SEARCH_GENERATED_COLORS_INFO_H_\n"
    "#define CHROME_COMMON_SEARCH_GENERATED_COLORS_INFO_H_\n"
    "\n"
    "#include <stdint.h>\n"
    "\n"
    "#include \"chrome/common/search/selected_colors_info.h\"\n"
    "#include \"third_party/skia/include/core/SkColor.h\"\n"
    "\n"
    "namespace chrome_colors {\n"
    "\n"
    "// List of preselected colors with icon data to show in Chrome Colors"
    " menu.\n"
    "constexpr ColorInfo kGeneratedColorsInfo[] = {\n"
    "$1\n"
    "};\n"
    "\n"
    "const size_t kNumColorsInfo = $2;"
    "\n"
    "} // namespace chrome_colors\n"
    "\n"
    "#endif  // CHROME_COMMON_SEARCH_GENERATED_COLORS_INFO_H_\n";

// Returns hex string representation for the |color| in "#FFFFFF" format.
std::string SkColorToHexString(SkColor color) {
  return base::StringPrintf("\"#%02X%02X%02X\"", SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color));
}

// Returns icon data for the given |color| as encoded svg.
// The returned string can be later directly set in JS with the following
// format: "data:image/svg+xml;base64<ENCODED_SVG>"
std::string GenerateIconDataForColor(SkColor color) {
  std::vector<std::string> subst;
  subst.push_back(SkColorToHexString(color));
  // TODO(gayane): Replace white will secondary color calculated using
  // browser_theme_pack.
  subst.push_back(SkColorToHexString(SK_ColorWHITE));

  std::string svg_base64;
  base::Base64Encode(
      base::ReplaceStringPlaceholders(kIconTemplate, subst, NULL), &svg_base64);
  return svg_base64;
}

// Generates color info line in the following format:
// ColorInfo(ID, SkColorSetRGB(R, G, B), LABEL, ICON_DATA)
std::string GenerateColorLine(chrome_colors::ColorInfo color_info) {
  std::vector<std::string> subst;
  subst.push_back(base::NumberToString(color_info.id));
  subst.push_back(base::NumberToString(SkColorGetR(color_info.color)));
  subst.push_back(base::NumberToString(SkColorGetG(color_info.color)));
  subst.push_back(base::NumberToString(SkColorGetB(color_info.color)));
  subst.push_back(color_info.label);
  subst.push_back(GenerateIconDataForColor(color_info.color));
  return base::ReplaceStringPlaceholders(kColorInfoLineTemplate, subst, NULL);
}

// Generates 'generated_colors_info.h' that contains selected colors from
// |chrome_colors::kSelectedColorsInfo| along with generated icon data.
void GenerateColorsInfoFile(std::string output_dir) {
  std::vector<std::string> updated_color_info;
  int colors_num = 0;
  for (chrome_colors::ColorInfo color_info :
       chrome_colors::kSelectedColorsInfo) {
    updated_color_info.push_back(GenerateColorLine(color_info));
    colors_num++;
  }

  std::vector<std::string> subst;
  subst.push_back(base::JoinString(updated_color_info, ",\n"));
  subst.push_back(base::NumberToString(colors_num));
  std::string output =
      base::ReplaceStringPlaceholders(kFileContentTemplate, subst, NULL);

  base::FilePath output_path = base::FilePath::FromUTF8Unsafe(output_dir);
  base::FilePath directory = output_path.DirName();
  if (!base::DirectoryExists(directory))
    base::CreateDirectory(directory);

  if (base::WriteFile(output_path, output.c_str(),
                      static_cast<uint32_t>(output.size())) <= 0) {
    LOG(ERROR) << "Failed to write output to " << output_path;
  }
}

int main(int argc, char* argv[]) {
  GenerateColorsInfoFile(argv[1]);
  return 0;
}
