// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/test/render_view_test.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_select_element.h"

using autofill::FormFieldData;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;

struct AutofillFieldLabelSourceCase {
  const char* description;
  const char* html;
  const FormFieldData::LabelSource label_source;
};

struct AutofillFieldUtilCase {
  const char* description;
  const char* html;
  const char* expected_label;
};

const char kElevenChildren[] =
    "<div id='target'>"
    "<div>child0</div>"
    "<div>child1</div>"
    "<div>child2</div>"
    "<div>child3</div>"
    "<div>child4</div>"
    "<div>child5</div>"
    "<div>child6</div>"
    "<div>child7</div>"
    "<div>child8</div>"
    "<div>child9</div>"
    "<div>child10</div>"
    "</div>";
const char kElevenChildrenExpected[] =
    "child0child1child2child3child4child5child6child7child8";

const char kElevenChildrenNested[] =
    "<div id='target'>"
    "<div>child0"
    "<div>child1"
    "<div>child2"
    "<div>child3"
    "<div>child4"
    "<div>child5"
    "<div>child6"
    "<div>child7"
    "<div>child8"
    "<div>child9"
    "<div>child10"
    "</div></div></div></div></div></div></div></div></div></div></div></div>";
// Take 10 elements -1 for target element, -1 as text is a leaf element.
const char kElevenChildrenNestedExpected[] = "child0child1child2child3child4";

const char kSkipElement[] =
    "<div id='target'>"
    "<div>child0</div>"
    "<div class='skip'>child1</div>"
    "<div>child2</div>"
    "</div>";
// TODO(crbug.com/796918): Should be child0child2
const char kSkipElementExpected[] = "child0";

const char kDivTableExample1[] =
    "<div>"
    "<div>label</div><div><input id='target'/></div>"
    "</div>";
const char kDivTableExample1Expected[] = "label";

const char kDivTableExample2[] =
    "<div>"
    "<div>label</div>"
    "<div>should be skipped<input/></div>"
    "<div><input id='target'/></div>"
    "</div>";
const char kDivTableExample2Expected[] = "label";

const char kDivTableExample3[] =
    "<div>"
    "<div>should be skipped<input/></div>"
    "<div>label</div>"
    "<div><input id='target'/></div>"
    "</div>";
const char kDivTableExample3Expected[] = "label";

const char kDivTableExample4[] =
    "<div>"
    "<div>should be skipped<input/></div>"
    "label"
    "<div><input id='target'/></div>"
    "</div>";
// TODO(crbug.com/796918): Should be label
const char kDivTableExample4Expected[] = "";

const char kDivTableExample5[] =
    "<div>"
    "<div>label<div><input id='target'/></div>behind</div>"
    "</div>";
// TODO(crbug.com/796918): Should be label
const char kDivTableExample5Expected[] = "labelbehind";

const char kDivTableExample6[] =
    "<div>"
    "<div>label<div><div>-<div><input id='target'/></div></div>"
    "</div>";
// TODO(crbug.com/796918): Should be "label" or "label-"
const char kDivTableExample6Expected[] = "";

class FormAutofillUtilsTest : public content::RenderViewTest {
 public:
  FormAutofillUtilsTest() : content::RenderViewTest() {}
  ~FormAutofillUtilsTest() override {}
};

TEST_F(FormAutofillUtilsTest, FindChildTextTest) {
  static const AutofillFieldUtilCase test_cases[] = {
      {"simple test", "<div id='target'>test</div>", "test"},
      {"Concatenate test", "<div id='target'><span>one</span>two</div>",
       "onetwo"},
      // TODO(crbug.com/796918): should be "onetwo"
      {"Ignore input", "<div id='target'>one<input value='test'/>two</div>",
       "one"},
      {"Trim", "<div id='target'>   one<span>two  </span></div>", "onetwo"},
      {"eleven children", kElevenChildren, kElevenChildrenExpected},
      // TODO(crbug.com/796918): Depth is only 5 elements
      {"eleven children nested", kElevenChildrenNested,
       kElevenChildrenNestedExpected},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = web_frame->GetDocument().GetElementById("target");
    ASSERT_FALSE(target.IsNull());
    EXPECT_EQ(base::UTF8ToUTF16(test_case.expected_label),
              autofill::form_util::FindChildText(target));
  }
}

TEST_F(FormAutofillUtilsTest, FindChildTextSkipElementTest) {
  static const AutofillFieldUtilCase test_cases[] = {
      {"Skip div element", kSkipElement, kSkipElementExpected},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = web_frame->GetDocument().GetElementById("target");
    ASSERT_FALSE(target.IsNull());
    WebVector<WebElement> web_to_skip =
        web_frame->GetDocument().QuerySelectorAll("div[class='skip']");
    std::set<blink::WebNode> to_skip;
    for (size_t i = 0; i < web_to_skip.size(); ++i) {
      to_skip.insert(web_to_skip[i]);
    }

    EXPECT_EQ(base::UTF8ToUTF16(test_case.expected_label),
              autofill::form_util::FindChildTextWithIgnoreListForTesting(
                  target, to_skip));
  }
}

TEST_F(FormAutofillUtilsTest, InferLabelForElementTest) {
  std::vector<base::char16> stop_words;
  stop_words.push_back(static_cast<base::char16>('-'));
  static const AutofillFieldUtilCase test_cases[] = {
      {"DIV table test 1", kDivTableExample1, kDivTableExample1Expected},
      {"DIV table test 2", kDivTableExample2, kDivTableExample2Expected},
      {"DIV table test 3", kDivTableExample3, kDivTableExample3Expected},
      {"DIV table test 4", kDivTableExample4, kDivTableExample4Expected},
      {"DIV table test 5", kDivTableExample5, kDivTableExample5Expected},
      {"DIV table test 6", kDivTableExample6, kDivTableExample6Expected},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = web_frame->GetDocument().GetElementById("target");
    ASSERT_FALSE(target.IsNull());
    const WebFormControlElement form_target =
        target.ToConst<WebFormControlElement>();
    ASSERT_FALSE(form_target.IsNull());

    FormFieldData::LabelSource label_source =
        FormFieldData::LabelSource::UNKNOWN;
    base::string16 label;
    autofill::form_util::InferLabelForElementForTesting(form_target, stop_words,
                                                        &label, &label_source);
    EXPECT_EQ(base::UTF8ToUTF16(test_case.expected_label), label);
  }
}

TEST_F(FormAutofillUtilsTest, InferLabelSourceTest) {
  const char kLabelSourceExpectedLabel[] = "label";
  static const AutofillFieldLabelSourceCase test_cases[] = {
      {"DIV_TABLE",
       "<div><div>label</div><div><input id='target'/></div></div>",
       FormFieldData::LabelSource::DIV_TABLE},
      {"LABEL_TAG", "<label>label</label><input id='target'/>",
       FormFieldData::LabelSource::LABEL_TAG},
      {"COMBINED", "<b>l</b><strong>a</strong>bel<input id='target'/>",
       FormFieldData::LabelSource::COMBINED},
      {"P_TAG", "<p><b>l</b><strong>a</strong>bel</p><input id='target'/>",
       FormFieldData::LabelSource::P_TAG},
      {"PLACE_HOLDER", "<input id='target' placeholder='label'/>",
       FormFieldData::LabelSource::PLACE_HOLDER},
      {"ARIA_LABEL", "<input id='target' aria-label='label'/>",
       FormFieldData::LabelSource::ARIA_LABEL},
      {"VALUE", "<input id='target' value='label'/>",
       FormFieldData::LabelSource::VALUE},
      {"LI_TAG", "<li>label<div><input id='target'/></div></li>",
       FormFieldData::LabelSource::LI_TAG},
      {"TD_TAG",
       "<table><tr><td>label</td><td><input id='target'/></td></tr></table>",
       FormFieldData::LabelSource::TD_TAG},
      {"DD_TAG", "<dl><dt>label</dt><dd><input id='target'></dd></dl>",
       FormFieldData::LabelSource::DD_TAG},
  };
  std::vector<base::char16> stop_words;
  stop_words.push_back(static_cast<base::char16>('-'));

  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = web_frame->GetDocument().GetElementById("target");
    ASSERT_FALSE(target.IsNull());
    const WebFormControlElement form_target =
        target.ToConst<WebFormControlElement>();
    ASSERT_FALSE(form_target.IsNull());

    FormFieldData::LabelSource label_source =
        FormFieldData::LabelSource::UNKNOWN;
    base::string16 label;
    EXPECT_TRUE(autofill::form_util::InferLabelForElementForTesting(
        form_target, stop_words, &label, &label_source));
    EXPECT_EQ(base::UTF8ToUTF16(kLabelSourceExpectedLabel), label);
    EXPECT_EQ(test_case.label_source, label_source);
  }
}
