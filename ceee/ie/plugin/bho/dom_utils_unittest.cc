// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unittests for DOM utils.
#include "ceee/ie/plugin/bho/dom_utils.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/mshtml_mocks.h"
#include "ceee/testing/utils/test_utils.h"

namespace {

using testing::_;
using testing::CopyInterfaceToArgument;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::StrEq;
using testing::VariantEq;

class MockDocument
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockDocument>,
      public StrictMock<IHTMLDocument2MockImpl>,
      public StrictMock<IHTMLDocument3MockImpl> {
 public:
  BEGIN_COM_MAP(MockDocument)
    COM_INTERFACE_ENTRY2(IDispatch, IHTMLDocument2)
    COM_INTERFACE_ENTRY(IHTMLDocument)
    COM_INTERFACE_ENTRY(IHTMLDocument2)
    COM_INTERFACE_ENTRY(IHTMLDocument3)
  END_COM_MAP()

  HRESULT Initialize(MockDocument** self) {
    *self = this;
    return S_OK;
  }
};

class MockElementNode
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockElementNode>,
      public StrictMock<IHTMLElementMockImpl>,
      public StrictMock<IHTMLDOMNodeMockImpl> {
  BEGIN_COM_MAP(MockElementNode)
    COM_INTERFACE_ENTRY2(IDispatch, IHTMLElement)
    COM_INTERFACE_ENTRY(IHTMLElement)
    COM_INTERFACE_ENTRY(IHTMLDOMNode)
  END_COM_MAP()

  HRESULT Initialize(MockElementNode** self) {
    *self = this;
    return S_OK;
  }
};

class MockStyleElementNode
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockStyleElementNode>,
      public StrictMock<IHTMLElementMockImpl>,
      public StrictMock<IHTMLStyleElementMockImpl>,
      public StrictMock<IHTMLDOMNodeMockImpl> {
  BEGIN_COM_MAP(MockStyleElementNode)
    COM_INTERFACE_ENTRY2(IDispatch, IHTMLElement)
    COM_INTERFACE_ENTRY(IHTMLElement)
    COM_INTERFACE_ENTRY(IHTMLStyleElement)
    COM_INTERFACE_ENTRY(IHTMLDOMNode)
  END_COM_MAP()

  HRESULT Initialize(MockStyleElementNode** self) {
    *self = this;
    return S_OK;
  }
};

class MockStyleSheet
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockStyleSheet>,
      public StrictMock<IHTMLStyleSheetMockImpl> {
  BEGIN_COM_MAP(MockStyleSheet)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLStyleSheet)
  END_COM_MAP()

  HRESULT Initialize(MockStyleSheet** self) {
    *self = this;
    return S_OK;
  }
};

class DomUtilsTest: public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(
        MockDocument::CreateInitialized(&document_, &document_keeper_));
    ASSERT_HRESULT_SUCCEEDED(
        MockElementNode::CreateInitialized(&head_node_, &head_node_keeper_));
  }

  virtual void TearDown() {
    document_ = NULL;
    document_keeper_.Release();
    head_node_ = NULL;
    head_node_keeper_.Release();
  }

 protected:
  MockDocument* document_;
  CComPtr<IHTMLDocument2> document_keeper_;

  MockElementNode* head_node_;
  CComPtr<IHTMLDOMNode> head_node_keeper_;
};

TEST_F(DomUtilsTest, InjectStyleTag) {
  MockStyleElementNode* style_node;
  CComPtr<IHTMLElement> style_node_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      MockStyleElementNode::CreateInitialized(&style_node, &style_node_keeper));

  MockStyleSheet* style_sheet;
  CComPtr<IHTMLStyleSheet> style_sheet_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      MockStyleSheet::CreateInitialized(&style_sheet, &style_sheet_keeper));

  EXPECT_CALL(*document_, createElement(StrEq(L"style"), _)).
      WillOnce(DoAll(CopyInterfaceToArgument<1>(style_node_keeper),
                     Return(S_OK)));

  EXPECT_CALL(*style_node, put_type(StrEq(L"text/css"))).
      WillOnce(Return(S_OK));

  EXPECT_CALL(*style_node, get_styleSheet(_)).
      WillOnce(DoAll(CopyInterfaceToArgument<0>(style_sheet_keeper),
                     Return(S_OK)));

  EXPECT_CALL(*style_sheet, put_cssText(StrEq(L"foo"))).WillOnce(Return(S_OK));

  EXPECT_CALL(*head_node_, appendChild(style_node, _)).
      WillOnce(Return(S_OK));

  ASSERT_HRESULT_SUCCEEDED(
      DomUtils::InjectStyleTag(document_keeper_, head_node_keeper_, L"foo"));
}

class MockElementCollection
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockElementCollection>,
      public StrictMock<IHTMLElementCollectionMockImpl> {
 public:
  BEGIN_COM_MAP(MockElementCollection)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLElementCollection)
  END_COM_MAP()

  HRESULT Initialize(MockElementCollection** self) {
    *self = this;
    return S_OK;
  }
};

TEST_F(DomUtilsTest, GetHeadNode) {
  MockElementCollection* collection;
  CComPtr<IHTMLElementCollection> collection_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      MockElementCollection::CreateInitialized(&collection,
                                               &collection_keeper));

  EXPECT_CALL(*document_, getElementsByTagName(StrEq(L"head"), _))
      .WillRepeatedly(DoAll(CopyInterfaceToArgument<1>(collection_keeper),
                            Return(S_OK)));

  CComVariant zero(0L);
  // First verify that we gracefuly fail when there are no heads.
  // bb2333090
  EXPECT_CALL(*collection, item(_, VariantEq(zero), _))
      .WillOnce(Return(S_OK));

  CComPtr<IHTMLDOMNode> head_node;
  ASSERT_HRESULT_FAILED(DomUtils::GetHeadNode(document_, &head_node));
  ASSERT_EQ(static_cast<IHTMLDOMNode*>(NULL), head_node);

  // And now properly return a valid head node.
  EXPECT_CALL(*collection, item(_, VariantEq(zero), _))
      .WillOnce(DoAll(CopyInterfaceToArgument<2>(
          static_cast<IDispatch*>(head_node_keeper_)), Return(S_OK)));

  ASSERT_HRESULT_SUCCEEDED(
      DomUtils::GetHeadNode(document_, &head_node));

  ASSERT_TRUE(head_node == head_node_);
}

}  // namespace
