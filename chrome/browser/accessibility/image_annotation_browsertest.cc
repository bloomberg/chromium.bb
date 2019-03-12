// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/image_annotation/public/cpp/image_processor.h"
#include "services/image_annotation/public/mojom/constants.mojom.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_tree.h"
#include "url/gurl.h"

constexpr base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/accessibility");

namespace {

void DescribeNodesWithAnnotations(const ui::AXNode& node,
                                  std::vector<std::string>* descriptions) {
  std::string annotation =
      node.GetStringAttribute(ax::mojom::StringAttribute::kImageAnnotation);
  if (!annotation.empty()) {
    descriptions->push_back(ui::ToString(node.data().role) + std::string(" ") +
                            annotation);
  }
  for (int i = 0; i < node.child_count(); i++)
    DescribeNodesWithAnnotations(*node.children()[i], descriptions);
}

std::vector<std::string> DescribeNodesWithAnnotations(
    const ui::AXTreeUpdate& tree_update) {
  ui::AXTree tree(tree_update);
  std::vector<std::string> descriptions;
  DCHECK(tree.root());
  DescribeNodesWithAnnotations(*tree.root(), &descriptions);
  return descriptions;
}

// A fake implementation of the Annotator mojo interface that
// returns predictable results based on the filename of the image
// it's asked to annotate. Enables us to test the rest of the
// system without using the real annotator that queries a back-end
// API.
class FakeAnnotator : public image_annotation::mojom::Annotator {
 public:
  FakeAnnotator() = default;
  ~FakeAnnotator() override = default;

  void BindRequest(image_annotation::mojom::AnnotatorRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  void AnnotateImage(const std::string& image_id,
                     image_annotation::mojom::ImageProcessorPtr image_processor,
                     AnnotateImageCallback callback) override {
    // Use the filename to create an annotation string.
    std::string image_filename = GURL(image_id).ExtractFileName();
    image_annotation::mojom::AnnotationPtr ocr_annotation =
        image_annotation::mojom::Annotation::New(
            image_annotation::mojom::AnnotationType::kOcr, 1.0,
            image_filename + " Annotation");

    // Return that result as an annotation.
    std::vector<image_annotation::mojom::AnnotationPtr> annotations;
    annotations.push_back(std::move(ocr_annotation));

    image_annotation::mojom::AnnotateImageResultPtr result =
        image_annotation::mojom::AnnotateImageResult::NewAnnotations(
            std::move(annotations));
    std::move(callback).Run(std::move(result));
  }

 private:
  mojo::BindingSet<image_annotation::mojom::Annotator> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeAnnotator);
};

// The fake ImageAnnotationService, which handles mojo calls from the renderer
// process and passes them to FakeAnnotator.
class FakeImageAnnotationService : public service_manager::Service {
 public:
  explicit FakeImageAnnotationService(
      service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {}

  ~FakeImageAnnotationService() override = default;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void OnStart() override {
    registry_.AddInterface<image_annotation::mojom::Annotator>(
        base::BindRepeating(&FakeAnnotator::BindRequest,
                            base::Unretained(&annotator_)));
  }

  service_manager::BinderRegistry registry_;
  service_manager::ServiceBinding service_binding_;

  FakeAnnotator annotator_;

  DISALLOW_COPY_AND_ASSIGN(FakeImageAnnotationService);
};

void HandleImageAnnotatorServiceRequest(
    service_manager::mojom::ServiceRequest request) {
  new FakeImageAnnotationService(std::move(request));
}

}  // namespace

class ImageAnnotationBrowserTest : public InProcessBrowserTest {
 public:
  ImageAnnotationBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
  }

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kExperimentalAccessibilityLabels);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(https_server_.Start());

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    content::ServiceManagerConnection* service_manager_connection =
        content::BrowserContext::GetServiceManagerConnectionFor(
            web_contents->GetBrowserContext());

    service_manager_connection->AddServiceRequestHandler(
        image_annotation::mojom::kServiceName,
        base::BindRepeating(&HandleImageAnnotatorServiceRequest));

    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kLabelImages, true);
    web_contents->SetAccessibilityMode(mode);
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ImageAnnotationBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest,
                       AnnotateImageInAccessibilityTree) {
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/image_annotation.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_contents, "Appears to say: red.png Annotation");
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest, ImagesInLinks) {
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/image_annotation_link.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_contents, "Appears to say: red.png Annotation");

  ui::AXTreeUpdate ax_tree_update =
      content::GetAccessibilityTreeSnapshot(web_contents);

  // All images should be annotated. Only links that contain exactly one image
  // should be annotated.

  EXPECT_THAT(
      DescribeNodesWithAnnotations(ax_tree_update),
      testing::ElementsAre("image Appears to say: red.png Annotation",
                           "link Appears to say: green.png Annotation",
                           "image Appears to say: green.png Annotation",
                           "image Appears to say: red.png Annotation",
                           "image Appears to say: printer.png Annotation",
                           "image Appears to say: red.png Annotation",
                           "link Appears to say: printer.png Annotation",
                           "image Appears to say: printer.png Annotation"));
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest, ImageDoc) {
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/image_annotation_doc.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_contents, "Appears to say: red.png Annotation");

  ui::AXTreeUpdate ax_tree_update =
      content::GetAccessibilityTreeSnapshot(web_contents);

  // When a document contains exactly one image, the document should be
  // annotated with the image's annotation, too.
  EXPECT_THAT(
      DescribeNodesWithAnnotations(ax_tree_update),
      testing::ElementsAre("rootWebArea Appears to say: red.png Annotation",
                           "image Appears to say: red.png Annotation"));
}
