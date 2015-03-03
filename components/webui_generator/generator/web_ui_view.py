# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import os
import os.path
import util
import html_view

H_FILE_TEMPLATE = \
"""// Copyright %(year)d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: this file is generated from "%(source)s". Do not modify directly.

#ifndef %(include_guard)s
#define %(include_guard)s

#include "base/macros.h"
#include "components/webui_generator/web_ui_view.h"
#include "%(export_h_include_path)s"

namespace gen {

class %(export_macro)s %(class_name)s : public ::webui_generator::WebUIView {
 public:
  %(class_name)s(content::WebUI* web_ui);
  %(class_name)s(content::WebUI* web_ui, const std::string& id);

 protected:
  void AddLocalizedValues(::login::LocalizedValuesBuilder* builder) override;
  void AddResources(ResourcesMap* resources_map) override;
  void CreateAndAddChildren() override;
  ::webui_generator::ViewModel* CreateViewModel() override;
  std::string GetType() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(%(class_name)s);
};

}  // namespace gen

#endif  // %(include_guard)s
"""

CC_FILE_TEMPLATE = \
"""// Copyright %(year)d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: this file is generated from "%(source)s". Do not modify directly.

#include "%(header_path)s"

#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_contents.h"
#include "components/login/localized_values_builder.h"
#include "grit/components_strings.h"
%(includes)s

namespace {

const char kHTMLDoc[] = "%(html_doc)s";
const char kJSDoc[] = "%(js_doc)s";

}  // namespace

namespace gen {

%(class_name)s::%(class_name)s(content::WebUI* web_ui)
    : webui_generator::WebUIView(web_ui, "WUG_ROOT") {
}

%(class_name)s::%(class_name)s(content::WebUI* web_ui, const std::string& id)
    : webui_generator::WebUIView(web_ui, id) {
}

void %(class_name)s::AddLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
%(add_localized_values_body)s
}

void %(class_name)s::AddResources(ResourcesMap* resources_map) {
%(add_resources_body)s
}

void %(class_name)s::CreateAndAddChildren() {
%(create_and_add_children_body)s
}

::webui_generator::ViewModel* %(class_name)s::CreateViewModel() {
%(create_view_model_body)s
}

std::string %(class_name)s::GetType() {
%(get_type_body)s
}

}  // namespace gen
"""

ADD_LOCALIZED_VALUE_TEMPLATE = \
"""  builder->Add("%(string_name)s", %(resource_id)s);"""

ADD_RESOURCE_TEMPLATE = \
"""  (*resources_map)["%(path)s"] = scoped_refptr<base::RefCountedMemory>(
    new base::RefCountedStaticMemory(%(const)s, arraysize(%(const)s) - 1));"""

CREATE_AND_ADD_CHILD_TEMPLATE = \
"""  AddChild(new gen::%(child_class)s(web_ui(), "%(child_id)s"));"""

CREATE_VIEW_MODEL_BODY_TEMPLATE = \
"""  return gen::%s::Create(web_ui()->GetWebContents()->GetBrowserContext());"""

def GetCommonSubistitutions(declaration):
  subs = {}
  subs['year'] = datetime.date.today().year
  subs['class_name'] = declaration.webui_view_class
  subs['source'] = declaration.path
  return subs


def GenHFile(declaration):
  subs = GetCommonSubistitutions(declaration)
  subs['include_guard'] = util.PathToIncludeGuard(
                              declaration.webui_view_include_path)
  subs['export_h_include_path'] = declaration.export_h_include_path
  subs['export_macro'] = declaration.component_export_macro
  return H_FILE_TEMPLATE % subs

def GenIncludes(declaration):
  lines = []
  lines.append('#include "%s"' % declaration.view_model_include_path)
  children_declarations = set(declaration.children.itervalues())
  for child in children_declarations:
    lines.append('#include "%s"' % child.webui_view_include_path)
  return '\n'.join(lines)

def GenAddLocalizedValuesBody(declaration):
  lines = []
  resource_id_prefix = "IDS_WUG_" + declaration.type.upper() + "_"
  for name in declaration.strings:
    resource_id = resource_id_prefix + name.upper()
    subs = {
      'string_name': util.ToLowerCamelCase(name),
      'resource_id': resource_id
    }
    lines.append(ADD_LOCALIZED_VALUE_TEMPLATE % subs)
  return '\n'.join(lines)

def GenAddResourcesBody(declaration):
  lines = []
  html_path = declaration.html_view_html_include_path
  lines.append(ADD_RESOURCE_TEMPLATE % { 'path': html_path,
                                         'const': 'kHTMLDoc' })
  js_path = declaration.html_view_js_include_path
  lines.append(ADD_RESOURCE_TEMPLATE % { 'path': js_path,
                                         'const': 'kJSDoc' })
  return '\n'.join(lines)

def GenCreateAndAddChildrenBody(children):
  lines = []
  for id, declaration in children.iteritems():
    subs = {
      'child_class': declaration.webui_view_class,
      'child_id': id
    }
    lines.append(CREATE_AND_ADD_CHILD_TEMPLATE % subs)
  return '\n'.join(lines)

def EscapeStringForCLiteral(string):
  return json.dumps(string)[1:][:-1]

def GenCCFile(declaration):
  subs = GetCommonSubistitutions(declaration)
  subs['header_path'] = declaration.webui_view_include_path
  subs['includes'] = GenIncludes(declaration)
  subs['add_localized_values_body'] = \
    GenAddLocalizedValuesBody(declaration)
  subs['add_resources_body'] = \
    GenAddResourcesBody(declaration)
  subs['create_and_add_children_body'] = \
    GenCreateAndAddChildrenBody(declaration.children)
  subs['create_view_model_body'] = \
    CREATE_VIEW_MODEL_BODY_TEMPLATE % declaration.view_model_class
  subs['get_type_body'] = '    return "%s";' % declaration.type
  subs['html_doc'] = EscapeStringForCLiteral(html_view.GenHTMLFile(declaration))
  subs['js_doc'] = EscapeStringForCLiteral(html_view.GenJSFile(declaration))
  return CC_FILE_TEMPLATE % subs

def ListOutputs(declaration, destination):
  dirname = os.path.join(destination, os.path.dirname(declaration.path))
  h_file_path = os.path.join(dirname, declaration.webui_view_h_name)
  cc_file_path = os.path.join(dirname, declaration.webui_view_cc_name)
  return [h_file_path, cc_file_path]

def Gen(declaration, destination):
  h_file_path, cc_file_path = ListOutputs(declaration, destination)
  util.CreateDirIfNotExists(os.path.dirname(h_file_path))
  open(h_file_path, 'w').write(GenHFile(declaration))
  open(cc_file_path, 'w').write(GenCCFile(declaration))

