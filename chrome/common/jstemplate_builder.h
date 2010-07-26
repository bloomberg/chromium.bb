// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides some helper methods for building and rendering an
// internal html page.  The flow is as follows:
// - instantiate a builder given a webframe that we're going to render content
//   into
// - load the template html and load the jstemplate javascript into the frame
// - given a json data object, run the jstemplate javascript which fills in
//   template values

#ifndef CHROME_COMMON_JSTEMPLATE_BUILDER_H_
#define CHROME_COMMON_JSTEMPLATE_BUILDER_H_
#pragma once

#include <string>

class DictionaryValue;
namespace base {
class StringPiece;
}

namespace jstemplate_builder {

// A helper function that generates a string of HTML to be loaded.  The
// string includes the HTML and the javascript code necessary to generate the
// full page with support for JsTemplates.
std::string GetTemplateHtml(const base::StringPiece& html_template,
                            const DictionaryValue* json,
                            const base::StringPiece& template_id);

// A helper function that generates a string of HTML to be loaded.  The
// string includes the HTML and the javascript code necessary to generate the
// full page with support for i18n Templates.
std::string GetI18nTemplateHtml(const base::StringPiece& html_template,
                                const DictionaryValue* json);

// A helper function that generates a string of HTML to be loaded.  The
// string includes the HTML and the javascript code necessary to generate the
// full page with support for both i18n Templates and JsTemplates.
std::string GetTemplatesHtml(const base::StringPiece& html_template,
                             const DictionaryValue* json,
                             const base::StringPiece& template_id);

// The following functions build up the different parts that the above
// templates use.

// Appends a script tag with a variable name |templateData| that has the JSON
// assigned to it.
void AppendJsonHtml(const DictionaryValue* json, std::string* output);

// Appends the source for JsTemplates in a script tag.
void AppendJsTemplateSourceHtml(std::string* output);

// Appends the code that processes the JsTemplate with the JSON. You should
// call AppendJsTemplateSourceHtml and AppendJsonHtml before calling this.
void AppendJsTemplateProcessHtml(const base::StringPiece& template_id,
                                 std::string* output);

// Appends the source for i18n Templates in a script tag.
void AppendI18nTemplateSourceHtml(std::string* output);

// Appends the code that processes the i18n Template with the JSON. You
// should call AppendJsTemplateSourceHtml and AppendJsonHtml before calling
// this.
void AppendI18nTemplateProcessHtml(std::string* output);

}  // namespace jstemplate_builder
#endif  // CHROME_COMMON_JSTEMPLATE_BUILDER_H_
