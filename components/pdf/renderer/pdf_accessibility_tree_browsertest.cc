// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/public/test/fake_pepper_plugin_instance.h"
#include "content/public/test/render_view_test.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace pdf {

namespace {

const PP_PrivateAccessibilityTextRunInfo kFirstTextRun = {
    15, PP_MakeFloatRectFromXYWH(26.0f, 189.0f, 84.0f, 13.0f)};
const PP_PrivateAccessibilityTextRunInfo kSecondTextRun = {
    15, PP_MakeFloatRectFromXYWH(28.0f, 117.0f, 152.0f, 19.0f)};
const PP_PrivateAccessibilityCharInfo kDummyCharsData[] = {
    {'H', 12}, {'e', 6},  {'l', 5},  {'l', 4},  {'o', 8},  {',', 4},
    {' ', 4},  {'w', 12}, {'o', 6},  {'r', 6},  {'l', 4},  {'d', 9},
    {'!', 4},  {' ', 0},  {' ', 0},  {'G', 16}, {'o', 12}, {'o', 12},
    {'d', 12}, {'b', 10}, {'y', 12}, {'e', 12}, {',', 4},  {' ', 6},
    {'w', 16}, {'o', 12}, {'r', 8},  {'l', 4},  {'d', 12}, {'!', 2},
};
const PP_PrivateAccessibilityTextRunInfo kFirstRunMultiLine = {
    7, PP_MakeFloatRectFromXYWH(26.0f, 189.0f, 84.0f, 13.0f)};
const PP_PrivateAccessibilityTextRunInfo kSecondRunMultiLine = {
    8, PP_MakeFloatRectFromXYWH(26.0f, 189.0f, 84.0f, 13.0f)};
const PP_PrivateAccessibilityTextRunInfo kThirdRunMultiLine = {
    9, PP_MakeFloatRectFromXYWH(26.0f, 189.0f, 84.0f, 13.0f)};
const PP_PrivateAccessibilityTextRunInfo kFourthRunMultiLine = {
    6, PP_MakeFloatRectFromXYWH(26.0f, 189.0f, 84.0f, 13.0f)};

const char kChromiumTestUrl[] = "www.cs.chromium.org";

void CompareRect(const PP_Rect& expected_rect, const PP_Rect& actual_rect) {
  EXPECT_EQ(expected_rect.point.x, actual_rect.point.x);
  EXPECT_EQ(expected_rect.point.y, actual_rect.point.y);
  EXPECT_EQ(expected_rect.size.height, actual_rect.size.height);
  EXPECT_EQ(expected_rect.size.width, actual_rect.size.width);
}

void CompareRect(const gfx::RectF& expected_rect,
                 const gfx::RectF& actual_rect) {
  EXPECT_FLOAT_EQ(expected_rect.x(), actual_rect.x());
  EXPECT_FLOAT_EQ(expected_rect.y(), actual_rect.y());
  EXPECT_FLOAT_EQ(expected_rect.size().height(), actual_rect.size().height());
  EXPECT_FLOAT_EQ(expected_rect.size().width(), actual_rect.size().width());
}

constexpr uint32_t MakeARGB(unsigned int a,
                            unsigned int r,
                            unsigned int g,
                            unsigned int b) {
  return (a << 24) | (r << 16) | (g << 8) | b;
}

// This class overrides content::FakePepperPluginInstance to record received
// action data when tests make an accessibility action call.
class ActionHandlingFakePepperPluginInstance
    : public content::FakePepperPluginInstance {
 public:
  ActionHandlingFakePepperPluginInstance() = default;
  ~ActionHandlingFakePepperPluginInstance() override = default;

  // content::FakePepperPluginInstance:
  void HandleAccessibilityAction(
      const PP_PdfAccessibilityActionData& action_data) override {
    received_action_data_ = action_data;
  }

  PP_PdfAccessibilityActionData GetReceivedActionData() {
    return received_action_data_;
  }

 private:
  PP_PdfAccessibilityActionData received_action_data_;
};

class FakeRendererPpapiHost : public content::RendererPpapiHost {
 public:
  explicit FakeRendererPpapiHost(content::RenderFrame* render_frame)
      : FakeRendererPpapiHost(render_frame, nullptr) {}
  FakeRendererPpapiHost(
      content::RenderFrame* render_frame,
      ActionHandlingFakePepperPluginInstance* fake_pepper_plugin_instance)
      : render_frame_(render_frame),
        fake_pepper_plugin_instance_(fake_pepper_plugin_instance) {}
  ~FakeRendererPpapiHost() override = default;

  ppapi::host::PpapiHost* GetPpapiHost() override { return nullptr; }
  bool IsValidInstance(PP_Instance instance) override { return true; }
  content::PepperPluginInstance* GetPluginInstance(
      PP_Instance instance) override {
    return fake_pepper_plugin_instance_;
  }
  content::RenderFrame* GetRenderFrameForInstance(
      PP_Instance instance) override {
    return render_frame_;
  }
  content::RenderView* GetRenderViewForInstance(PP_Instance instance) override {
    return nullptr;
  }
  blink::WebPluginContainer* GetContainerForInstance(
      PP_Instance instance) override {
    return nullptr;
  }
  bool HasUserGesture(PP_Instance instance) override { return false; }
  int GetRoutingIDForWidget(PP_Instance instance) override { return 0; }
  gfx::Point PluginPointToRenderFrame(PP_Instance instance,
                                      const gfx::Point& pt) override {
    return gfx::Point();
  }
  IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      bool should_close_source) override {
    return IPC::PlatformFileForTransit();
  }
  base::UnsafeSharedMemoryRegion ShareUnsafeSharedMemoryRegionWithRemote(
      const base::UnsafeSharedMemoryRegion& region) override {
    return base::UnsafeSharedMemoryRegion();
  }
  base::ReadOnlySharedMemoryRegion ShareReadOnlySharedMemoryRegionWithRemote(
      const base::ReadOnlySharedMemoryRegion& region) override {
    return base::ReadOnlySharedMemoryRegion();
  }
  bool IsRunningInProcess() override { return false; }
  std::string GetPluginName() override { return std::string(); }
  void SetToExternalPluginHost() override {}
  void CreateBrowserResourceHosts(
      PP_Instance instance,
      const std::vector<IPC::Message>& nested_msgs,
      base::OnceCallback<void(const std::vector<int>&)> callback) override {}
  GURL GetDocumentURL(PP_Instance instance) override { return GURL(); }

 private:
  content::RenderFrame* render_frame_;
  ActionHandlingFakePepperPluginInstance* fake_pepper_plugin_instance_;
};

}  // namespace

class PdfAccessibilityTreeTest : public content::RenderViewTest {
 public:
  PdfAccessibilityTreeTest() {}
  ~PdfAccessibilityTreeTest() override = default;

  void SetUp() override {
    content::RenderViewTest::SetUp();

    base::FilePath pak_dir;
    base::PathService::Get(base::DIR_MODULE, &pak_dir);
    base::FilePath pak_file =
        pak_dir.Append(FILE_PATH_LITERAL("components_tests_resources.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_file, ui::SCALE_FACTOR_NONE);

    viewport_info_.zoom = 1.0;
    viewport_info_.scale = 1.0;
    viewport_info_.scroll = {0, 0};
    viewport_info_.offset = {0, 0};
    viewport_info_.selection_start_page_index = 0;
    viewport_info_.selection_start_char_index = 0;
    viewport_info_.selection_end_page_index = 0;
    viewport_info_.selection_end_char_index = 0;
    doc_info_.page_count = 1;
    page_info_.page_index = 0;
    page_info_.text_run_count = 0;
    page_info_.char_count = 0;
    page_info_.bounds = PP_MakeRectFromXYWH(0, 0, 1, 1);
  }

 protected:
  PP_PrivateAccessibilityViewportInfo viewport_info_;
  PP_PrivateAccessibilityDocInfo doc_info_;
  PP_PrivateAccessibilityPageInfo page_info_;
  std::vector<ppapi::PdfAccessibilityTextRunInfo> text_runs_;
  std::vector<PP_PrivateAccessibilityCharInfo> chars_;
  ppapi::PdfAccessibilityPageObjects page_objects_;
};

TEST_F(PdfAccessibilityTreeTest, TestEmptyPDFPage) {
  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  EXPECT_EQ(ax::mojom::Role::kDocument,
            pdf_accessibility_tree.GetRoot()->data().role);
}

TEST_F(PdfAccessibilityTreeTest, TestAccessibilityDisabledDuringPDFLoad) {
  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);

  // Disable accessibility while the PDF is loading, make sure this
  // doesn't crash.
  render_frame->SetAccessibilityModeForTest(ui::AXMode());

  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
}

TEST_F(PdfAccessibilityTreeTest, TestPdfAccessibilityTreeCreation) {
  static const char kTestAltText[] = "Alternate text for image";

  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    ppapi::PdfAccessibilityLinkInfo link;
    link.bounds = PP_MakeFloatRectFromXYWH(1.0f, 1.0f, 5.0f, 6.0f);
    link.url = kChromiumTestUrl;
    link.text_run_index = 0;
    link.text_run_count = 1;
    page_objects_.links.push_back(std::move(link));
  }

  {
    ppapi::PdfAccessibilityImageInfo image;
    image.bounds = PP_MakeFloatRectFromXYWH(8.0f, 9.0f, 2.0f, 1.0f);
    image.alt_text = kTestAltText;
    image.text_run_index = 2;
    page_objects_.images.push_back(std::move(image));
  }

  {
    ppapi::PdfAccessibilityImageInfo image;
    image.bounds = PP_MakeFloatRectFromXYWH(11.0f, 14.0f, 5.0f, 8.0f);
    image.text_run_index = 2;
    page_objects_.images.push_back(std::move(image));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  pdf::PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Link
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++ Image
   * ++++++ Image
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kDocument, root_node->data().role);
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->data().role);
  ASSERT_EQ(2u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->data().role);
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(1u, paragraph_node->children().size());

  ui::AXNode* link_node = paragraph_node->children()[0];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->data().role);
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            link_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, link_node->children().size());

  paragraph_node = page_node->children()[1];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->data().role);
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(3u, paragraph_node->children().size());

  ui::AXNode* static_text_node = paragraph_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->data().role);
  ASSERT_EQ(1u, static_text_node->children().size());

  ui::AXNode* image_node = paragraph_node->children()[1];
  ASSERT_TRUE(image_node);
  EXPECT_EQ(ax::mojom::Role::kImage, image_node->data().role);
  EXPECT_EQ(gfx::RectF(8.0f, 9.0f, 2.0f, 1.0f),
            image_node->data().relative_bounds.bounds);
  EXPECT_EQ(kTestAltText,
            image_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  image_node = paragraph_node->children()[2];
  ASSERT_TRUE(image_node);
  EXPECT_EQ(ax::mojom::Role::kImage, image_node->data().role);
  EXPECT_EQ(gfx::RectF(11.0f, 14.0f, 5.0f, 8.0f),
            image_node->data().relative_bounds.bounds);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION),
            image_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PdfAccessibilityTreeTest, TestOverlappingAnnots) {
  text_runs_.emplace_back(kFirstRunMultiLine);
  text_runs_.emplace_back(kSecondRunMultiLine);
  text_runs_.emplace_back(kThirdRunMultiLine);
  text_runs_.emplace_back(kFourthRunMultiLine);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    ppapi::PdfAccessibilityLinkInfo link;
    link.bounds = PP_MakeFloatRectFromXYWH(1.0f, 1.0f, 5.0f, 6.0f);
    link.url = kChromiumTestUrl;
    link.text_run_index = 0;
    link.text_run_count = 3;
    page_objects_.links.push_back(std::move(link));
  }

  {
    ppapi::PdfAccessibilityLinkInfo link;
    link.bounds = PP_MakeFloatRectFromXYWH(1.0f, 2.0f, 5.0f, 6.0f);
    link.url = kChromiumTestUrl;
    link.text_run_index = 1;
    link.text_run_count = 2;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  pdf::PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Link
   * ++++++ Link
   * ++++++ Static Text
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kDocument, root_node->data().role);
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->data().role);
  ASSERT_EQ(1u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->data().role);
  const std::vector<ui::AXNode*>& child_nodes = paragraph_node->children();
  ASSERT_EQ(3u, child_nodes.size());

  ui::AXNode* link_node = child_nodes[0];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->data().role);
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            link_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, link_node->children().size());

  link_node = child_nodes[1];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->data().role);
  EXPECT_EQ(gfx::RectF(1.0f, 2.0f, 5.0f, 6.0f),
            link_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, link_node->children().size());

  ui::AXNode* static_text_node = child_nodes[2];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->data().role);
  ASSERT_EQ(1u, static_text_node->children().size());
}

TEST_F(PdfAccessibilityTreeTest, TestHighlightCreation) {
  // Enable feature flag
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome_pdf::features::kAccessiblePDFHighlight);

  constexpr uint32_t kHighlightWhiteColor = MakeARGB(255, 255, 255, 255);

  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    ppapi::PdfAccessibilityHighlightInfo highlight;
    highlight.bounds = PP_MakeFloatRectFromXYWH(1.0f, 1.0f, 5.0f, 6.0f);
    highlight.text_run_index = 0;
    highlight.text_run_count = 2;
    highlight.color = kHighlightWhiteColor;
    page_objects_.highlights.push_back(std::move(highlight));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  pdf::PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Highlight
   * ++++++++ Static Text
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kDocument, root_node->data().role);
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->data().role);
  ASSERT_EQ(1u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->data().role);
  ASSERT_EQ(1u, paragraph_node->children().size());

  ui::AXNode* highlight_node = paragraph_node->children()[0];
  ASSERT_TRUE(highlight_node);
  EXPECT_EQ(ax::mojom::Role::kPdfActionableHighlight,
            highlight_node->data().role);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_AX_ROLE_DESCRIPTION_PDF_HIGHLIGHT),
            highlight_node->GetStringAttribute(
                ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            highlight_node->data().relative_bounds.bounds);
  ASSERT_TRUE(highlight_node->HasIntAttribute(
      ax::mojom::IntAttribute::kBackgroundColor));
  EXPECT_EQ(kHighlightWhiteColor,
            static_cast<uint32_t>(highlight_node->GetIntAttribute(
                ax::mojom::IntAttribute::kBackgroundColor)));
  ASSERT_EQ(1u, highlight_node->children().size());

  ui::AXNode* static_text_node = highlight_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->data().role);
  ASSERT_EQ(2u, static_text_node->children().size());
}

TEST_F(PdfAccessibilityTreeTest, TestTextFieldNodeCreation) {
  // Enable feature flag
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome_pdf::features::kAccessiblePDFForm);
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    ppapi::PdfAccessibilityTextFieldInfo text_field;
    text_field.bounds = PP_MakeFloatRectFromXYWH(1.0f, 1.0f, 5.0f, 6.0f);
    text_field.index_in_page = 0;
    text_field.text_run_index = 2;
    text_field.name = "Text Box";
    text_field.value = "Text";
    text_field.is_read_only = false;
    text_field.is_required = false;
    text_field.is_password = false;
    page_objects_.text_fields.push_back(std::move(text_field));
  }

  {
    ppapi::PdfAccessibilityTextFieldInfo text_field;
    text_field.bounds = PP_MakeFloatRectFromXYWH(1.0f, 10.0f, 5.0f, 6.0f);
    text_field.index_in_page = 1;
    text_field.text_run_index = 2;
    text_field.name = "Text Box 2";
    text_field.value = "Text 2";
    text_field.is_read_only = true;
    text_field.is_required = true;
    text_field.is_password = true;
    page_objects_.text_fields.push_back(std::move(text_field));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  pdf::PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++ Text Field
   * ++++++ Text Field
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kDocument, root_node->data().role);
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->data().role);
  ASSERT_EQ(2u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->data().role);
  ASSERT_EQ(1u, paragraph_node->children().size());

  ui::AXNode* static_text_node = paragraph_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->data().role);
  ASSERT_EQ(1u, static_text_node->children().size());

  paragraph_node = page_node->children()[1];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->data().role);
  const std::vector<ui::AXNode*>& child_nodes = paragraph_node->children();
  ASSERT_EQ(3u, child_nodes.size());

  static_text_node = child_nodes[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->data().role);
  ASSERT_EQ(1u, static_text_node->children().size());

  ui::AXNode* text_field_node = child_nodes[1];
  ASSERT_TRUE(text_field_node);
  EXPECT_EQ(ax::mojom::Role::kTextField, text_field_node->data().role);
  EXPECT_EQ("Text Box", text_field_node->GetStringAttribute(
                            ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Text", text_field_node->GetStringAttribute(
                        ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(text_field_node->data().HasState(ax::mojom::State::kRequired));
  EXPECT_FALSE(text_field_node->data().HasState(ax::mojom::State::kProtected));
  EXPECT_NE(ax::mojom::Restriction::kReadOnly,
            text_field_node->data().GetRestriction());
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            text_field_node->data().relative_bounds.bounds);
  EXPECT_EQ(0u, text_field_node->children().size());

  text_field_node = child_nodes[2];
  ASSERT_TRUE(text_field_node);
  EXPECT_EQ(ax::mojom::Role::kTextField, text_field_node->data().role);
  EXPECT_EQ("Text Box 2", text_field_node->GetStringAttribute(
                              ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Text 2", text_field_node->GetStringAttribute(
                          ax::mojom::StringAttribute::kValue));
  EXPECT_TRUE(text_field_node->data().HasState(ax::mojom::State::kRequired));
  EXPECT_TRUE(text_field_node->data().HasState(ax::mojom::State::kProtected));
  EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
            text_field_node->data().GetRestriction());
  EXPECT_EQ(gfx::RectF(1.0f, 10.0f, 5.0f, 6.0f),
            text_field_node->data().relative_bounds.bounds);
  EXPECT_EQ(0u, text_field_node->children().size());
}

TEST_F(PdfAccessibilityTreeTest, TestPreviousNextOnLine) {
  text_runs_.emplace_back(kFirstRunMultiLine);
  text_runs_.emplace_back(kSecondRunMultiLine);
  text_runs_.emplace_back(kThirdRunMultiLine);
  text_runs_.emplace_back(kFourthRunMultiLine);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    ppapi::PdfAccessibilityLinkInfo link;
    link.bounds = PP_MakeFloatRectFromXYWH(0.0f, 0.0f, 0.0f, 0.0f);
    link.url = kChromiumTestUrl;
    link.text_run_index = 2;
    link.text_run_count = 2;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  pdf::PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++++ Inline Text Box
   * ++++++++ Inline Text Box
   * ++++++ Link
   * ++++++++ Static Text
   * ++++++++++ Inline Text Box
   * ++++++++++ Inline Text Box
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kDocument, root_node->data().role);
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->data().role);
  ASSERT_EQ(1u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->data().role);
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(2u, paragraph_node->children().size());

  ui::AXNode* static_text_node = paragraph_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->data().role);
  ASSERT_EQ(2u, static_text_node->children().size());

  ui::AXNode* previous_inline_node = static_text_node->children()[0];
  ASSERT_TRUE(previous_inline_node);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, previous_inline_node->data().role);
  ASSERT_FALSE(previous_inline_node->HasIntAttribute(
      ax::mojom::IntAttribute::kPreviousOnLineId));

  ui::AXNode* next_inline_node = static_text_node->children()[1];
  ASSERT_TRUE(next_inline_node);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, next_inline_node->data().role);
  ASSERT_TRUE(next_inline_node->HasIntAttribute(
      ax::mojom::IntAttribute::kNextOnLineId));

  ASSERT_EQ(next_inline_node->data().id,
            previous_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kNextOnLineId));
  ASSERT_EQ(previous_inline_node->data().id,
            next_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kPreviousOnLineId));

  ui::AXNode* link_node = paragraph_node->children()[1];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->data().role);
  ASSERT_EQ(1u, link_node->children().size());

  static_text_node = link_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->data().role);
  ASSERT_EQ(2u, static_text_node->children().size());

  previous_inline_node = static_text_node->children()[0];
  ASSERT_TRUE(previous_inline_node);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, previous_inline_node->data().role);
  ASSERT_TRUE(previous_inline_node->HasIntAttribute(
      ax::mojom::IntAttribute::kPreviousOnLineId));
  // Test that text and link on the same line are connected.
  ASSERT_EQ(next_inline_node->data().id,
            previous_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kPreviousOnLineId));

  next_inline_node = static_text_node->children()[1];
  ASSERT_TRUE(next_inline_node);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, next_inline_node->data().role);
  ASSERT_FALSE(next_inline_node->HasIntAttribute(
      ax::mojom::IntAttribute::kNextOnLineId));

  ASSERT_EQ(next_inline_node->data().id,
            previous_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kNextOnLineId));
  ASSERT_EQ(previous_inline_node->data().id,
            next_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kPreviousOnLineId));
}

TEST_F(PdfAccessibilityTreeTest, TextRunsAndCharsMismatch) {
  // |chars_| and |text_runs_| span over the same page text. They should denote
  // the same page text size, but |text_runs_| is incorrect and only denotes 1
  // of 2 text runs.
  text_runs_.emplace_back(kFirstTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_EQ(ax::mojom::Role::kUnknown,
            pdf_accessibility_tree.GetRoot()->data().role);
  ASSERT_EQ(0u, pdf_accessibility_tree.GetRoot()->children().size());
}

TEST_F(PdfAccessibilityTreeTest, UnsortedLinkVector) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    // Add first link in the vector.
    ppapi::PdfAccessibilityLinkInfo link;
    link.bounds = PP_MakeFloatRectFromXYWH(0.0f, 0.0f, 0.0f, 0.0f);
    link.text_run_index = 2;
    link.text_run_count = 0;
    page_objects_.links.push_back(std::move(link));
  }

  {
    // Add second link in the vector.
    ppapi::PdfAccessibilityLinkInfo link;
    link.bounds = PP_MakeFloatRectFromXYWH(0.0f, 0.0f, 0.0f, 0.0f);
    link.text_run_index = 0;
    link.text_run_count = 1;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_EQ(ax::mojom::Role::kUnknown,
            pdf_accessibility_tree.GetRoot()->data().role);
  ASSERT_EQ(0u, pdf_accessibility_tree.GetRoot()->children().size());
}

TEST_F(PdfAccessibilityTreeTest, OutOfBoundLink) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    ppapi::PdfAccessibilityLinkInfo link;
    link.bounds = PP_MakeFloatRectFromXYWH(0.0f, 0.0f, 0.0f, 0.0f);
    link.text_run_index = 3;
    link.text_run_count = 0;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_EQ(ax::mojom::Role::kUnknown,
            pdf_accessibility_tree.GetRoot()->data().role);
  ASSERT_EQ(0u, pdf_accessibility_tree.GetRoot()->children().size());
}

TEST_F(PdfAccessibilityTreeTest, UnsortedImageVector) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    // Add first image to the vector.
    ppapi::PdfAccessibilityImageInfo image;
    image.bounds = PP_MakeFloatRectFromXYWH(0.0f, 0.0f, 0.0f, 0.0f);
    image.text_run_index = 1;
    page_objects_.images.push_back(std::move(image));
  }

  {
    // Add second image to the vector.
    ppapi::PdfAccessibilityImageInfo image;
    image.bounds = PP_MakeFloatRectFromXYWH(0.0f, 0.0f, 0.0f, 0.0f);
    image.text_run_index = 0;
    page_objects_.images.push_back(std::move(image));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_EQ(ax::mojom::Role::kUnknown,
            pdf_accessibility_tree.GetRoot()->data().role);
  ASSERT_EQ(0u, pdf_accessibility_tree.GetRoot()->children().size());
}

TEST_F(PdfAccessibilityTreeTest, OutOfBoundImage) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    ppapi::PdfAccessibilityImageInfo image;
    image.bounds = PP_MakeFloatRectFromXYWH(0.0f, 0.0f, 0.0f, 0.0f);
    image.text_run_index = 3;
    page_objects_.images.push_back(std::move(image));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_EQ(ax::mojom::Role::kUnknown,
            pdf_accessibility_tree.GetRoot()->data().role);
  ASSERT_EQ(0u, pdf_accessibility_tree.GetRoot()->children().size());
}

TEST_F(PdfAccessibilityTreeTest, UnsortedHighlightVector) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    // Add first highlight in the vector.
    ppapi::PdfAccessibilityHighlightInfo highlight;
    highlight.bounds = PP_MakeFloatRectFromXYWH(0.0f, 0.0f, 1.0f, 1.0f);
    highlight.text_run_index = 2;
    highlight.text_run_count = 0;
    page_objects_.highlights.push_back(std::move(highlight));
  }

  {
    // Add second highlight in the vector.
    ppapi::PdfAccessibilityHighlightInfo highlight;
    highlight.bounds = PP_MakeFloatRectFromXYWH(2.0f, 2.0f, 1.0f, 1.0f);
    highlight.text_run_index = 0;
    highlight.text_run_count = 1;
    page_objects_.highlights.push_back(std::move(highlight));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_EQ(ax::mojom::Role::kUnknown,
            pdf_accessibility_tree.GetRoot()->data().role);
  ASSERT_EQ(0u, pdf_accessibility_tree.GetRoot()->children().size());
}

TEST_F(PdfAccessibilityTreeTest, OutOfBoundHighlight) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    ppapi::PdfAccessibilityHighlightInfo highlight;
    highlight.bounds = PP_MakeFloatRectFromXYWH(0.0f, 0.0f, 1.0f, 1.0f);
    highlight.text_run_index = 3;
    highlight.text_run_count = 0;
    page_objects_.highlights.push_back(std::move(highlight));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  FakeRendererPpapiHost host(view_->GetMainRenderFrame());
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_EQ(ax::mojom::Role::kUnknown,
            pdf_accessibility_tree.GetRoot()->data().role);
  ASSERT_EQ(0u, pdf_accessibility_tree.GetRoot()->children().size());
}

TEST_F(PdfAccessibilityTreeTest, TestActionDataConversion) {
  // This test verifies the AXActionData conversion to
  // PP_AccessibilityActionData.
  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  ActionHandlingFakePepperPluginInstance fake_pepper_instance;
  FakeRendererPpapiHost host(view_->GetMainRenderFrame(),
                             &fake_pepper_instance);
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree.CreateActionTarget(*root_node);
  ASSERT_TRUE(pdf_action_target);
  EXPECT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50), ax::mojom::ScrollAlignment::kScrollAlignmentLeft,
      ax::mojom::ScrollAlignment::kScrollAlignmentTop,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  PP_PdfAccessibilityActionData action_data =
      fake_pepper_instance.GetReceivedActionData();
  EXPECT_EQ(PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_MAKE_VISIBLE,
            action_data.action);
  EXPECT_EQ(PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT,
            action_data.horizontal_scroll_alignment);
  EXPECT_EQ(PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP,
            action_data.vertical_scroll_alignment);

  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50),
      ax::mojom::ScrollAlignment::kScrollAlignmentRight,
      ax::mojom::ScrollAlignment::kScrollAlignmentTop,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  action_data = fake_pepper_instance.GetReceivedActionData();
  EXPECT_EQ(PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT,
            action_data.horizontal_scroll_alignment);

  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50),
      ax::mojom::ScrollAlignment::kScrollAlignmentBottom,
      ax::mojom::ScrollAlignment::kScrollAlignmentBottom,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  action_data = fake_pepper_instance.GetReceivedActionData();
  EXPECT_EQ(PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM,
            action_data.horizontal_scroll_alignment);

  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50),
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  action_data = fake_pepper_instance.GetReceivedActionData();
  EXPECT_EQ(PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER,
            action_data.horizontal_scroll_alignment);
  EXPECT_EQ(
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CLOSEST_EDGE,
      action_data.vertical_scroll_alignment);
  CompareRect({{0, 0}, {1, 1}}, action_data.target_rect);
}

TEST_F(PdfAccessibilityTreeTest, TestScrollToGlobalPointDataConversion) {
  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  ActionHandlingFakePepperPluginInstance fake_pepper_instance;
  FakeRendererPpapiHost host(view_->GetMainRenderFrame(),
                             &fake_pepper_instance);
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree.CreateActionTarget(*root_node);
  ASSERT_TRUE(pdf_action_target);
  EXPECT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  EXPECT_TRUE(pdf_action_target->ScrollToGlobalPoint(gfx::Point(50, 50)));

  PP_PdfAccessibilityActionData action_data =
      fake_pepper_instance.GetReceivedActionData();
  EXPECT_EQ(PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_GLOBAL_POINT,
            action_data.action);
  EXPECT_EQ(50, action_data.target_point.x);
  EXPECT_EQ(50, action_data.target_point.y);
  CompareRect({{0, 0}, {1, 1}}, action_data.target_rect);
}

TEST_F(PdfAccessibilityTreeTest, TestClickActionDataConversion) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    ppapi::PdfAccessibilityLinkInfo link;
    link.url = kChromiumTestUrl;
    link.text_run_index = 0;
    link.text_run_count = 1;
    link.bounds = {{0, 0}, {10, 10}};
    link.index_in_page = 0;
    page_objects_.links.push_back(std::move(link));
  }

  {
    ppapi::PdfAccessibilityLinkInfo link;
    link.url = kChromiumTestUrl;
    link.text_run_index = 1;
    link.text_run_count = 1;
    link.bounds = {{10, 10}, {10, 10}};
    link.index_in_page = 1;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  ActionHandlingFakePepperPluginInstance fake_pepper_instance;
  FakeRendererPpapiHost host(view_->GetMainRenderFrame(),
                             &fake_pepper_instance);
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  const std::vector<ui::AXNode*>& page_nodes = root_node->children();
  ASSERT_EQ(1u, page_nodes.size());
  const std::vector<ui::AXNode*>& para_nodes = page_nodes[0]->children();
  ASSERT_EQ(2u, para_nodes.size());
  const std::vector<ui::AXNode*>& link_nodes = para_nodes[1]->children();
  ASSERT_EQ(1u, link_nodes.size());

  const ui::AXNode* link_node = link_nodes[0];
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree.CreateActionTarget(*link_node);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  pdf_action_target->Click();
  PP_PdfAccessibilityActionData pdf_action_data =
      fake_pepper_instance.GetReceivedActionData();

  EXPECT_EQ(PP_PdfAccessibilityAction::PP_PDF_DO_DEFAULT_ACTION,
            pdf_action_data.action);
  EXPECT_EQ(PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE,
            pdf_action_data.horizontal_scroll_alignment);
  EXPECT_EQ(PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE,
            pdf_action_data.vertical_scroll_alignment);
  EXPECT_EQ(0u, pdf_action_data.page_index);
  EXPECT_EQ(PP_PdfAccessibilityAnnotationType::PP_PDF_LINK,
            pdf_action_data.annotation_type);
  EXPECT_EQ(1u, pdf_action_data.annotation_index);
  CompareRect({{0, 0}, {0, 0}}, pdf_action_data.target_rect);
}

TEST_F(PdfAccessibilityTreeTest, TestEmptyPdfAxActions) {
  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  ActionHandlingFakePepperPluginInstance fake_pepper_instance;
  FakeRendererPpapiHost host(view_->GetMainRenderFrame(),
                             &fake_pepper_instance);
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree.CreateActionTarget(*root_node);
  ASSERT_TRUE(pdf_action_target);
  EXPECT_FALSE(pdf_action_target->ClearAccessibilityFocus());
  EXPECT_FALSE(pdf_action_target->Click());
  EXPECT_FALSE(pdf_action_target->Decrement());
  EXPECT_FALSE(pdf_action_target->Increment());
  EXPECT_FALSE(pdf_action_target->Focus());
  gfx::Rect rect = pdf_action_target->GetRelativeBounds();
  EXPECT_TRUE(rect.origin().IsOrigin());
  EXPECT_TRUE(rect.IsEmpty());

  gfx::Point point = pdf_action_target->GetScrollOffset();
  EXPECT_EQ(point.x(), 0);
  EXPECT_EQ(point.y(), 0);

  point = pdf_action_target->MinimumScrollOffset();
  EXPECT_EQ(point.x(), 0);
  EXPECT_EQ(point.y(), 0);

  point = pdf_action_target->MaximumScrollOffset();
  EXPECT_EQ(point.x(), 0);
  EXPECT_EQ(point.y(), 0);

  EXPECT_FALSE(pdf_action_target->SetAccessibilityFocus());
  EXPECT_FALSE(pdf_action_target->SetSelected(true));
  EXPECT_FALSE(pdf_action_target->SetSelected(false));
  EXPECT_FALSE(pdf_action_target->SetSequentialFocusNavigationStartingPoint());
  EXPECT_FALSE(pdf_action_target->SetValue("test"));
  EXPECT_FALSE(pdf_action_target->ShowContextMenu());
  EXPECT_FALSE(pdf_action_target->ScrollToMakeVisible());
}

TEST_F(PdfAccessibilityTreeTest, TestZoomAndScaleChanges) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  ActionHandlingFakePepperPluginInstance fake_pepper_instance;
  FakeRendererPpapiHost host(view_->GetMainRenderFrame(),
                             &fake_pepper_instance);
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);

  viewport_info_.zoom = 1.0;
  viewport_info_.scale = 1.0;
  viewport_info_.scroll = {0, -56};
  viewport_info_.offset = {57, 0};

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  ASSERT_EQ(1u, root_node->children().size());
  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  ASSERT_EQ(2u, page_node->children().size());
  ui::AXNode* para_node = page_node->children()[0];
  ASSERT_TRUE(para_node);
  gfx::RectF rect = para_node->data().relative_bounds.bounds;
  CompareRect({{26.0f, 189.0f}, {84.0f, 13.0f}}, rect);
  gfx::Transform* transform = root_node->data().relative_bounds.transform.get();
  ASSERT_TRUE(transform);
  transform->TransformRect(&rect);
  CompareRect({{83.0f, 245.0f}, {84.0f, 13.0f}}, rect);

  float new_device_scale = 1.5f;
  float new_zoom = 1.5f;
  viewport_info_.zoom = new_zoom;
  viewport_info_.scale = new_device_scale;
  SetUseZoomForDSFEnabled(true);
  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);

  rect = para_node->data().relative_bounds.bounds;
  transform = root_node->data().relative_bounds.transform.get();
  ASSERT_TRUE(transform);
  transform->TransformRect(&rect);
  CompareRect({{186.75f, 509.25f}, {189.00f, 29.25f}}, rect);

  SetUseZoomForDSFEnabled(false);
  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);

  rect = para_node->data().relative_bounds.bounds;
  transform = root_node->data().relative_bounds.transform.get();
  ASSERT_TRUE(transform);
  transform->TransformRect(&rect);
  CompareRect({{124.5f, 339.5f}, {126.0f, 19.5f}}, rect);
}

TEST_F(PdfAccessibilityTreeTest, TestSelectionActionDataConversion) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));
  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();
  content::RenderFrame* render_frame = view_->GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());
  ActionHandlingFakePepperPluginInstance fake_pepper_instance;
  FakeRendererPpapiHost host(view_->GetMainRenderFrame(),
                             &fake_pepper_instance);
  PP_Instance instance = 0;
  PdfAccessibilityTree pdf_accessibility_tree(&host, instance);
  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  const std::vector<ui::AXNode*>& page_nodes = root_node->children();
  ASSERT_EQ(1u, page_nodes.size());
  ASSERT_TRUE(page_nodes[0]);
  const std::vector<ui::AXNode*>& para_nodes = page_nodes[0]->children();
  ASSERT_EQ(2u, para_nodes.size());
  ASSERT_TRUE(para_nodes[0]);
  const std::vector<ui::AXNode*>& static_text_nodes1 =
      para_nodes[0]->children();
  ASSERT_EQ(1u, static_text_nodes1.size());
  ASSERT_TRUE(static_text_nodes1[0]);
  const std::vector<ui::AXNode*>& inline_text_nodes1 =
      static_text_nodes1[0]->children();
  ASSERT_TRUE(inline_text_nodes1[0]);
  ASSERT_EQ(1u, inline_text_nodes1.size());
  ASSERT_TRUE(para_nodes[1]);
  const std::vector<ui::AXNode*>& static_text_nodes2 =
      para_nodes[1]->children();
  ASSERT_EQ(1u, static_text_nodes2.size());
  ASSERT_TRUE(static_text_nodes2[0]);
  const std::vector<ui::AXNode*>& inline_text_nodes2 =
      static_text_nodes2[0]->children();
  ASSERT_TRUE(inline_text_nodes2[0]);
  ASSERT_EQ(1u, inline_text_nodes2.size());

  std::unique_ptr<ui::AXActionTarget> pdf_anchor_action_target =
      pdf_accessibility_tree.CreateActionTarget(*inline_text_nodes1[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf,
            pdf_anchor_action_target->GetType());
  std::unique_ptr<ui::AXActionTarget> pdf_focus_action_target =
      pdf_accessibility_tree.CreateActionTarget(*inline_text_nodes2[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_focus_action_target->GetType());
  EXPECT_TRUE(pdf_anchor_action_target->SetSelection(
      pdf_anchor_action_target.get(), 1, pdf_focus_action_target.get(), 5));

  PP_PdfAccessibilityActionData pdf_action_data =
      fake_pepper_instance.GetReceivedActionData();
  EXPECT_EQ(PP_PdfAccessibilityAction::PP_PDF_SET_SELECTION,
            pdf_action_data.action);
  EXPECT_EQ(0u, pdf_action_data.selection_start_index.page_index);
  EXPECT_EQ(1u, pdf_action_data.selection_start_index.char_index);
  EXPECT_EQ(0u, pdf_action_data.selection_end_index.page_index);
  EXPECT_EQ(20u, pdf_action_data.selection_end_index.char_index);

  pdf_anchor_action_target =
      pdf_accessibility_tree.CreateActionTarget(*static_text_nodes1[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf,
            pdf_anchor_action_target->GetType());
  pdf_focus_action_target =
      pdf_accessibility_tree.CreateActionTarget(*inline_text_nodes2[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_focus_action_target->GetType());
  EXPECT_TRUE(pdf_anchor_action_target->SetSelection(
      pdf_anchor_action_target.get(), 1, pdf_focus_action_target.get(), 4));

  pdf_action_data = fake_pepper_instance.GetReceivedActionData();
  EXPECT_EQ(PP_PdfAccessibilityAction::PP_PDF_SET_SELECTION,
            pdf_action_data.action);
  EXPECT_EQ(0u, pdf_action_data.selection_start_index.page_index);
  EXPECT_EQ(1u, pdf_action_data.selection_start_index.char_index);
  EXPECT_EQ(0u, pdf_action_data.selection_end_index.page_index);
  EXPECT_EQ(19u, pdf_action_data.selection_end_index.char_index);

  pdf_anchor_action_target =
      pdf_accessibility_tree.CreateActionTarget(*para_nodes[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf,
            pdf_anchor_action_target->GetType());
  pdf_focus_action_target =
      pdf_accessibility_tree.CreateActionTarget(*para_nodes[1]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_focus_action_target->GetType());
  EXPECT_FALSE(pdf_anchor_action_target->SetSelection(
      pdf_anchor_action_target.get(), 1, pdf_focus_action_target.get(), 5));
}

}  // namespace pdf
