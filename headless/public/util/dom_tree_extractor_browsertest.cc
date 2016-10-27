// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/dom_tree_extractor.h"

#include <memory>
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/domains/emulation.h"
#include "headless/public/domains/network.h"
#include "headless/public/domains/page.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace headless {

class DomTreeExtractorBrowserTest : public HeadlessAsyncDevTooledBrowserTest,
                                    public page::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/dom_tree_test.html").spec());
  }

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    devtools_client_->GetPage()->RemoveObserver(this);

    extractor_.reset(new DomTreeExtractor(devtools_client_.get()));

    std::vector<std::string> css_whitelist = {
        "color",        "display",    "font-style",   "margin-left",
        "margin-right", "margin-top", "margin-bottom"};
    extractor_->ExtractDomTree(
        css_whitelist,
        base::Bind(&DomTreeExtractorBrowserTest::OnDomTreeExtracted,
                   base::Unretained(this)));
  }

  void OnDomTreeExtracted(DomTreeExtractor::DomTree dom_tree) {
    GURL::Replacements replace_port;
    replace_port.SetPortStr("");

    std::vector<std::unique_ptr<base::DictionaryValue>> dom_nodes(
        dom_tree.dom_nodes_.size());

    // For convenience flatten the dom tree into an array.
    for (size_t i = 0; i < dom_tree.dom_nodes_.size(); i++) {
      dom::Node* node = const_cast<dom::Node*>(dom_tree.dom_nodes_[i]);

      dom_nodes[i].reset(
          static_cast<base::DictionaryValue*>(node->Serialize().release()));

      // Convert child & content document pointers into indexes.
      if (node->HasChildren()) {
        std::unique_ptr<base::ListValue> children(new base::ListValue());
        for (const std::unique_ptr<dom::Node>& child : *node->GetChildren()) {
          children->AppendInteger(
              dom_tree.node_id_to_index_[child->GetNodeId()]);
        }
        dom_nodes[i]->Set("childIndices", std::move(children));
        dom_nodes[i]->Remove("children", nullptr);
      }

      if (node->HasContentDocument()) {
        dom_nodes[i]->SetInteger(
            "contentDocumentIndex",
            dom_tree
                .node_id_to_index_[node->GetContentDocument()->GetNodeId()]);
        dom_nodes[i]->Remove("contentDocument", nullptr);
      }

      dom_nodes[i]->Remove("childNodeCount", nullptr);

      // Frame IDs are random.
      if (dom_nodes[i]->HasKey("frameId"))
        dom_nodes[i]->SetString("frameId", "?");

      // Ports are random.
      std::string url;
      if (dom_nodes[i]->GetString("baseURL", &url)) {
        dom_nodes[i]->SetString(
            "baseURL", GURL(url).ReplaceComponents(replace_port).spec());
      }

      if (dom_nodes[i]->GetString("documentURL", &url)) {
        dom_nodes[i]->SetString(
            "documentURL", GURL(url).ReplaceComponents(replace_port).spec());
      }
    }

    // Merge LayoutTreeNode data into the dictionaries.
    for (const css::LayoutTreeNode* layout_node : dom_tree.layout_tree_nodes_) {
      auto it = dom_tree.node_id_to_index_.find(layout_node->GetNodeId());
      ASSERT_TRUE(it != dom_tree.node_id_to_index_.end());

      base::DictionaryValue* node_dict = dom_nodes[it->second].get();
      node_dict->Set("boundingBox", layout_node->GetBoundingBox()->Serialize());

      if (layout_node->HasLayoutText())
        node_dict->SetString("layoutText", layout_node->GetLayoutText());

      if (layout_node->HasStyleIndex())
        node_dict->SetInteger("styleIndex", layout_node->GetStyleIndex());

      if (layout_node->HasInlineTextNodes()) {
        std::unique_ptr<base::ListValue> inline_text_nodes(
            new base::ListValue());
        for (const std::unique_ptr<css::InlineTextBox>& inline_text_box :
             *layout_node->GetInlineTextNodes()) {
          size_t index = inline_text_nodes->GetSize();
          inline_text_nodes->Set(index, inline_text_box->Serialize());
        }
        node_dict->Set("inlineTextNodes", std::move(inline_text_nodes));
      }
    }

    std::vector<std::unique_ptr<base::DictionaryValue>> computed_styles(
        dom_tree.computed_styles_.size());

    for (size_t i = 0; i < dom_tree.computed_styles_.size(); i++) {
      std::unique_ptr<base::DictionaryValue> style(new base::DictionaryValue());
      for (const auto& style_property :
           *dom_tree.computed_styles_[i]->GetProperties()) {
        style->SetString(style_property->GetName(), style_property->GetValue());
      }
      computed_styles[i] = std::move(style);
    }

    const std::vector<std::string> expected_dom_nodes = {
        "{\n"
        "   'baseURL': 'http://127.0.0.1/dom_tree_test.html',\n"
        "   'boundingBox': {\n"
        "      'height': 600.0,\n"
        "      'width': 800.0,\n"
        "      'x': 0.0,\n"
        "      'y': 0.0\n"
        "   },\n"
        "   'childIndices': [ 1 ],\n"
        "   'documentURL': 'http://127.0.0.1/dom_tree_test.html',\n"
        "   'localName': '',\n"
        "   'nodeId': 1,\n"
        "   'nodeName': '#document',\n"
        "   'nodeType': 9,\n"
        "   'nodeValue': '',\n"
        "   'xmlVersion': ''\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'boundingBox': {\n"
        "      'height': 600.0,\n"
        "      'width': 800.0,\n"
        "      'x': 0.0,\n"
        "      'y': 0.0\n"
        "   },\n"
        "   'childIndices': [ 2, 5 ],\n"
        "   'frameId': '?',\n"
        "   'localName': 'html',\n"
        "   'nodeId': 2,\n"
        "   'nodeName': 'HTML',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 0\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'childIndices': [ 3 ],\n"
        "   'localName': 'head',\n"
        "   'nodeId': 3,\n"
        "   'nodeName': 'HEAD',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': ''\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'childIndices': [ 4 ],\n"
        "   'localName': 'title',\n"
        "   'nodeId': 4,\n"
        "   'nodeName': 'TITLE',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': ''\n"
        "}\n",

        "{\n"
        "   'localName': '',\n"
        "   'nodeId': 5,\n"
        "   'nodeName': '#text',\n"
        "   'nodeType': 3,\n"
        "   'nodeValue': 'Hello world!'\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'boundingBox': {\n"
        "      'height': 584.0,\n"
        "      'width': 784.0,\n"
        "      'x': 8.0,\n"
        "      'y': 8.0\n"
        "   },\n"
        "   'childIndices': [ 6 ],\n"
        "   'localName': 'body',\n"
        "   'nodeId': 6,\n"
        "   'nodeName': 'BODY',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 1\n"
        "}\n",

        "{\n"
        "   'attributes': [ 'id', 'id1' ],\n"
        "   'boundingBox': {\n"
        "      'height': 367.0,\n"
        "      'width': 784.0,\n"
        "      'x': 8.0,\n"
        "      'y': 8.0\n"
        "   },\n"
        "   'childIndices': [ 7, 9, 16 ],\n"
        "   'localName': 'div',\n"
        "   'nodeId': 7,\n"
        "   'nodeName': 'DIV',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 0\n"
        "}\n",

        "{\n"
        "   'attributes': [ 'style', 'color: red' ],\n"
        "   'boundingBox': {\n"
        "      'height': 37.0,\n"
        "      'width': 784.0,\n"
        "      'x': 8.0,\n"
        "      'y': 8.0\n"
        "   },\n"
        "   'childIndices': [ 8 ],\n"
        "   'localName': 'h1',\n"
        "   'nodeId': 8,\n"
        "   'nodeName': 'H1',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 2\n"
        "}\n",

        "{\n"
        "   'boundingBox': {\n"
        "      'height': 36.0,\n"
        "      'width': 143.0,\n"
        "      'x': 8.0,\n"
        "      'y': 8.0\n"
        "   },\n"
        "   'inlineTextNodes': [ {\n"
        "      'boundingBox': {\n"
        "         'height': 36.0,\n"
        "         'width': 142.171875,\n"
        "         'x': 8.0,\n"
        "         'y': 8.0\n"
        "      },\n"
        "      'numCharacters': 10,\n"
        "      'startCharacterIndex': 0\n"
        "   } ],\n"
        "   'layoutText': 'Some text.',\n"
        "   'localName': '',\n"
        "   'nodeId': 9,\n"
        "   'nodeName': '#text',\n"
        "   'nodeType': 3,\n"
        "   'nodeValue': 'Some text.',\n"
        "   'styleIndex': 2\n"
        "}\n",

        "{\n"
        "   'attributes': [ 'src', '/iframe.html', 'width', '400', 'height', "
        "'200' ],\n"
        "   'boundingBox': {\n"
        "      'height': 205.0,\n"
        "      'width': 404.0,\n"
        "      'x': 8.0,\n"
        "      'y': 66.0\n"
        "   },\n"
        "   'childIndices': [  ],\n"
        "   'contentDocumentIndex': 10,\n"
        "   'frameId': '?',\n"
        "   'localName': 'iframe',\n"
        "   'nodeId': 10,\n"
        "   'nodeName': 'IFRAME',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 4\n"
        "}\n",

        "{\n"
        "   'baseURL': 'http://127.0.0.1/iframe.html',\n"
        "   'childIndices': [ 11 ],\n"
        "   'documentURL': 'http://127.0.0.1/iframe.html',\n"
        "   'localName': '',\n"
        "   'nodeId': 11,\n"
        "   'nodeName': '#document',\n"
        "   'nodeType': 9,\n"
        "   'nodeValue': '',\n"
        "   'xmlVersion': ''\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'boundingBox': {\n"
        "      'height': 200.0,\n"
        "      'width': 400.0,\n"
        "      'x': 10.0,\n"
        "      'y': 68.0\n"
        "   },\n"
        "   'childIndices': [ 12, 13 ],\n"
        "   'frameId': '?',\n"
        "   'localName': 'html',\n"
        "   'nodeId': 12,\n"
        "   'nodeName': 'HTML',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 0\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'childIndices': [  ],\n"
        "   'localName': 'head',\n"
        "   'nodeId': 13,\n"
        "   'nodeName': 'HEAD',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': ''\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'boundingBox': {\n"
        "      'height': 171.0,\n"
        "      'width': 384.0,\n"
        "      'x': 18.0,\n"
        "      'y': 76.0\n"
        "   },\n"
        "   'childIndices': [ 14 ],\n"
        "   'localName': 'body',\n"
        "   'nodeId': 14,\n"
        "   'nodeName': 'BODY',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 1\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'boundingBox': {\n"
        "      'height': 37.0,\n"
        "      'width': 384.0,\n"
        "      'x': 18.0,\n"
        "      'y': 76.0\n"
        "   },\n"
        "   'childIndices': [ 15 ],\n"
        "   'localName': 'h1',\n"
        "   'nodeId': 15,\n"
        "   'nodeName': 'H1',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 3\n"
        "}\n",

        "{\n"
        "   'boundingBox': {\n"
        "      'height': 36.0,\n"
        "      'width': 308.0,\n"
        "      'x': 8.0,\n"
        "      'y': 8.0\n"
        "   },\n"
        "   'inlineTextNodes': [ {\n"
        "      'boundingBox': {\n"
        "         'height': 36.0,\n"
        "         'width': 307.734375,\n"
        "         'x': 8.0,\n"
        "         'y': 8.0\n"
        "      },\n"
        "      'numCharacters': 22,\n"
        "      'startCharacterIndex': 0\n"
        "   } ],\n"
        "   'layoutText': 'Hello from the iframe!',\n"
        "   'localName': '',\n"
        "   'nodeId': 16,\n"
        "   'nodeName': '#text',\n"
        "   'nodeType': 3,\n"
        "   'nodeValue': 'Hello from the iframe!',\n"
        "   'styleIndex': 3\n"
        "}\n",

        "{\n"
        "   'attributes': [ 'id', 'id2' ],\n"
        "   'boundingBox': {\n"
        "      'height': 105.0,\n"
        "      'width': 784.0,\n"
        "      'x': 8.0,\n"
        "      'y': 270.0\n"
        "   },\n"
        "   'childIndices': [ 17 ],\n"
        "   'localName': 'div',\n"
        "   'nodeId': 17,\n"
        "   'nodeName': 'DIV',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 0\n"
        "}\n",

        "{\n"
        "   'attributes': [ 'id', 'id3' ],\n"
        "   'boundingBox': {\n"
        "      'height': 105.0,\n"
        "      'width': 784.0,\n"
        "      'x': 8.0,\n"
        "      'y': 270.0\n"
        "   },\n"
        "   'childIndices': [ 18 ],\n"
        "   'localName': 'div',\n"
        "   'nodeId': 18,\n"
        "   'nodeName': 'DIV',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 0\n"
        "}\n",

        "{\n"
        "   'attributes': [ 'id', 'id4' ],\n"
        "   'boundingBox': {\n"
        "      'height': 105.0,\n"
        "      'width': 784.0,\n"
        "      'x': 8.0,\n"
        "      'y': 270.0\n"
        "   },\n"
        "   'childIndices': [ 19, 21, 23, 24 ],\n"
        "   'localName': 'div',\n"
        "   'nodeId': 19,\n"
        "   'nodeName': 'DIV',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 0\n"
        "}\n",

        "{\n"
        "   'attributes': [ 'href', 'https://www.google.com' ],\n"
        "   'boundingBox': {\n"
        "      'height': 18.0,\n"
        "      'width': 53.0,\n"
        "      'x': 8.0,\n"
        "      'y': 270.0\n"
        "   },\n"
        "   'childIndices': [ 20 ],\n"
        "   'localName': 'a',\n"
        "   'nodeId': 20,\n"
        "   'nodeName': 'A',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 5\n"
        "}\n",

        "{\n"
        "   'boundingBox': {\n"
        "      'height': 18.0,\n"
        "      'width': 53.0,\n"
        "      'x': 8.0,\n"
        "      'y': 270.0\n"
        "   },\n"
        "   'inlineTextNodes': [ {\n"
        "      'boundingBox': {\n"
        "         'height': 17.0,\n"
        "         'width': 52.421875,\n"
        "         'x': 8.0,\n"
        "         'y': 270.4375\n"
        "      },\n"
        "      'numCharacters': 7,\n"
        "      'startCharacterIndex': 0\n"
        "   } ],\n"
        "   'layoutText': 'Google!',\n"
        "   'localName': '',\n"
        "   'nodeId': 21,\n"
        "   'nodeName': '#text',\n"
        "   'nodeType': 3,\n"
        "   'nodeValue': 'Google!',\n"
        "   'styleIndex': 5\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'boundingBox': {\n"
        "      'height': 19.0,\n"
        "      'width': 784.0,\n"
        "      'x': 8.0,\n"
        "      'y': 304.0\n"
        "   },\n"
        "   'childIndices': [ 22 ],\n"
        "   'localName': 'p',\n"
        "   'nodeId': 22,\n"
        "   'nodeName': 'P',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 6\n"
        "}\n",

        "{\n"
        "   'boundingBox': {\n"
        "      'height': 18.0,\n"
        "      'width': 85.0,\n"
        "      'x': 8.0,\n"
        "      'y': 304.0\n"
        "   },\n"
        "   'inlineTextNodes': [ {\n"
        "      'boundingBox': {\n"
        "         'height': 17.0,\n"
        "         'width': 84.84375,\n"
        "         'x': 8.0,\n"
        "         'y': 304.4375\n"
        "      },\n"
        "      'numCharacters': 12,\n"
        "      'startCharacterIndex': 0\n"
        "   } ],\n"
        "   'layoutText': 'A paragraph!',\n"
        "   'localName': '',\n"
        "   'nodeId': 23,\n"
        "   'nodeName': '#text',\n"
        "   'nodeType': 3,\n"
        "   'nodeValue': 'A paragraph!',\n"
        "   'styleIndex': 6\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'boundingBox': {\n"
        "      'height': 0.0,\n"
        "      'width': 0.0,\n"
        "      'x': 0.0,\n"
        "      'y': 0.0\n"
        "   },\n"
        "   'childIndices': [  ],\n"
        "   'inlineTextNodes': [ {\n"
        "      'boundingBox': {\n"
        "         'height': 17.0,\n"
        "         'width': 0.0,\n"
        "         'x': 8.0,\n"
        "         'y': 338.4375\n"
        "      },\n"
        "      'numCharacters': 1,\n"
        "      'startCharacterIndex': 0\n"
        "   } ],\n"
        "   'layoutText': '\\n',\n"
        "   'localName': 'br',\n"
        "   'nodeId': 24,\n"
        "   'nodeName': 'BR',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 4\n"
        "}\n",

        "{\n"
        "   'attributes': [ 'style', 'color: green' ],\n"
        "   'boundingBox': {\n"
        "      'height': 19.0,\n"
        "      'width': 784.0,\n"
        "      'x': 8.0,\n"
        "      'y': 356.0\n"
        "   },\n"
        "   'childIndices': [ 25, 26, 28 ],\n"
        "   'localName': 'div',\n"
        "   'nodeId': 25,\n"
        "   'nodeName': 'DIV',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 7\n"
        "}\n",

        "{\n"
        "   'boundingBox': {\n"
        "      'height': 18.0,\n"
        "      'width': 41.0,\n"
        "      'x': 8.0,\n"
        "      'y': 356.0\n"
        "   },\n"
        "   'inlineTextNodes': [ {\n"
        "      'boundingBox': {\n"
        "         'height': 17.0,\n"
        "         'width': 40.4375,\n"
        "         'x': 8.0,\n"
        "         'y': 356.4375\n"
        "      },\n"
        "      'numCharacters': 5,\n"
        "      'startCharacterIndex': 0\n"
        "   } ],\n"
        "   'layoutText': 'Some ',\n"
        "   'localName': '',\n"
        "   'nodeId': 26,\n"
        "   'nodeName': '#text',\n"
        "   'nodeType': 3,\n"
        "   'nodeValue': 'Some ',\n"
        "   'styleIndex': 7\n"
        "}\n",

        "{\n"
        "   'attributes': [  ],\n"
        "   'boundingBox': {\n"
        "      'height': 18.0,\n"
        "      'width': 37.0,\n"
        "      'x': 48.0,\n"
        "      'y': 356.0\n"
        "   },\n"
        "   'childIndices': [ 27 ],\n"
        "   'localName': 'em',\n"
        "   'nodeId': 27,\n"
        "   'nodeName': 'EM',\n"
        "   'nodeType': 1,\n"
        "   'nodeValue': '',\n"
        "   'styleIndex': 8\n"
        "}\n",

        "{\n"
        "   'boundingBox': {\n"
        "      'height': 18.0,\n"
        "      'width': 37.0,\n"
        "      'x': 48.0,\n"
        "      'y': 356.0\n"
        "   },\n"
        "   'inlineTextNodes': [ {\n"
        "      'boundingBox': {\n"
        "         'height': 17.0,\n"
        "         'width': 35.828125,\n"
        "         'x': 48.4375,\n"
        "         'y': 356.4375\n"
        "      },\n"
        "      'numCharacters': 5,\n"
        "      'startCharacterIndex': 0\n"
        "   } ],\n"
        "   'layoutText': 'green',\n"
        "   'localName': '',\n"
        "   'nodeId': 28,\n"
        "   'nodeName': '#text',\n"
        "   'nodeType': 3,\n"
        "   'nodeValue': 'green',\n"
        "   'styleIndex': 8\n"
        "}\n",

        "{\n"
        "   'boundingBox': {\n"
        "      'height': 18.0,\n"
        "      'width': 41.0,\n"
        "      'x': 84.0,\n"
        "      'y': 356.0\n"
        "   },\n"
        "   'inlineTextNodes': [ {\n"
        "      'boundingBox': {\n"
        "         'height': 17.0,\n"
        "         'width': 39.984375,\n"
        "         'x': 84.265625,\n"
        "         'y': 356.4375\n"
        "      },\n"
        "      'numCharacters': 8,\n"
        "      'startCharacterIndex': 0\n"
        "   } ],\n"
        "   'layoutText': ' text...',\n"
        "   'localName': '',\n"
        "   'nodeId': 29,\n"
        "   'nodeName': '#text',\n"
        "   'nodeType': 3,\n"
        "   'nodeValue': ' text...',\n"
        "   'styleIndex': 7\n"
        "}\n"};

    EXPECT_EQ(expected_dom_nodes.size(), dom_nodes.size());

    for (size_t i = 0; i < dom_nodes.size(); i++) {
      std::string result_json;
      base::JSONWriter::WriteWithOptions(
          *dom_nodes[i], base::JSONWriter::OPTIONS_PRETTY_PRINT, &result_json);

      base::ReplaceChars(result_json, "\"", "'", &result_json);

      ASSERT_LT(i, expected_dom_nodes.size());
      EXPECT_EQ(expected_dom_nodes[i], result_json) << " Node # " << i;
    }

    const std::vector<std::string> expected_styles = {
        "{\n"
        "   'color': 'rgb(0, 0, 0)',\n"
        "   'display': 'block',\n"
        "   'font-style': 'normal',\n"
        "   'margin-bottom': '0px',\n"
        "   'margin-left': '0px',\n"
        "   'margin-right': '0px',\n"
        "   'margin-top': '0px'\n"
        "}\n",

        "{\n"
        "   'color': 'rgb(0, 0, 0)',\n"
        "   'display': 'block',\n"
        "   'font-style': 'normal',\n"
        "   'margin-bottom': '8px',\n"
        "   'margin-left': '8px',\n"
        "   'margin-right': '8px',\n"
        "   'margin-top': '8px'\n"
        "}\n",

        "{\n"
        "   'color': 'rgb(255, 0, 0)',\n"
        "   'display': 'block',\n"
        "   'font-style': 'normal',\n"
        "   'margin-bottom': '21.44px',\n"
        "   'margin-left': '0px',\n"
        "   'margin-right': '0px',\n"
        "   'margin-top': '21.44px'\n"
        "}\n",

        "{\n"
        "   'color': 'rgb(0, 0, 0)',\n"
        "   'display': 'block',\n"
        "   'font-style': 'normal',\n"
        "   'margin-bottom': '21.44px',\n"
        "   'margin-left': '0px',\n"
        "   'margin-right': '0px',\n"
        "   'margin-top': '21.44px'\n"
        "}\n",

        "{\n"
        "   'color': 'rgb(0, 0, 0)',\n"
        "   'display': 'inline',\n"
        "   'font-style': 'normal',\n"
        "   'margin-bottom': '0px',\n"
        "   'margin-left': '0px',\n"
        "   'margin-right': '0px',\n"
        "   'margin-top': '0px'\n"
        "}\n",

        "{\n"
        "   'color': 'rgb(0, 0, 238)',\n"
        "   'display': 'inline',\n"
        "   'font-style': 'normal',\n"
        "   'margin-bottom': '0px',\n"
        "   'margin-left': '0px',\n"
        "   'margin-right': '0px',\n"
        "   'margin-top': '0px'\n"
        "}\n",

        "{\n"
        "   'color': 'rgb(0, 0, 0)',\n"
        "   'display': 'block',\n"
        "   'font-style': 'normal',\n"
        "   'margin-bottom': '16px',\n"
        "   'margin-left': '0px',\n"
        "   'margin-right': '0px',\n"
        "   'margin-top': '16px'\n"
        "}\n",

        "{\n"
        "   'color': 'rgb(0, 128, 0)',\n"
        "   'display': 'block',\n"
        "   'font-style': 'normal',\n"
        "   'margin-bottom': '0px',\n"
        "   'margin-left': '0px',\n"
        "   'margin-right': '0px',\n"
        "   'margin-top': '0px'\n"
        "}\n",

        "{\n"
        "   'color': 'rgb(0, 128, 0)',\n"
        "   'display': 'inline',\n"
        "   'font-style': 'italic',\n"
        "   'margin-bottom': '0px',\n"
        "   'margin-left': '0px',\n"
        "   'margin-right': '0px',\n"
        "   'margin-top': '0px'\n"
        "}\n"};

    for (size_t i = 0; i < computed_styles.size(); i++) {
      std::string result_json;
      base::JSONWriter::WriteWithOptions(*computed_styles[i],
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                         &result_json);

      base::ReplaceChars(result_json, "\"", "'", &result_json);

      ASSERT_LT(i, expected_styles.size());
      EXPECT_EQ(expected_styles[i], result_json) << " Style # " << i;
    }

    FinishAsynchronousTest();
  }

  std::unique_ptr<DomTreeExtractor> extractor_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(DomTreeExtractorBrowserTest);

}  // namespace headless
