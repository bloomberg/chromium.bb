// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_test_utils.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kUrl1Title[] = "url1";
const char kUrl1Url[] = "http://www.url1.com";
const char kUrl2Title[] = "url2";
const char kUrl2Url[] = "http://www.url2.com";
const char kUrl3Title[] = "url3";
const char kUrl3Url[] = "http://www.url3.com";
const char kUrl4Title[] = "url4";
const char kUrl4Url[] = "http://www.url4.com";
const char kFolder1Title[] = "folder1";
const char kFolder2Title[] = "folder2";

// Helper to get a mutable bookmark node.
static BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

}  // anonymous namespace

class BookmarkCodecTest : public testing::Test {
 protected:
  // Helpers to create bookmark models with different data.
  BookmarkModel* CreateTestModel1() {
    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    const BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
    model->AddURL(bookmark_bar, 0, ASCIIToUTF16(kUrl1Title), GURL(kUrl1Url));
    return model.release();
  }
  BookmarkModel* CreateTestModel2() {
    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    const BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
    model->AddURL(bookmark_bar, 0, ASCIIToUTF16(kUrl1Title), GURL(kUrl1Url));
    model->AddURL(bookmark_bar, 1, ASCIIToUTF16(kUrl2Title), GURL(kUrl2Url));
    return model.release();
  }
  BookmarkModel* CreateTestModel3() {
    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    const BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
    model->AddURL(bookmark_bar, 0, ASCIIToUTF16(kUrl1Title), GURL(kUrl1Url));
    const BookmarkNode* folder1 = model->AddFolder(bookmark_bar, 1,
                                                   ASCIIToUTF16(kFolder1Title));
    model->AddURL(folder1, 0, ASCIIToUTF16(kUrl2Title), GURL(kUrl2Url));
    return model.release();
  }

  void GetBookmarksBarChildValue(Value* value,
                                 size_t index,
                                 DictionaryValue** result_value) {
    ASSERT_EQ(Value::TYPE_DICTIONARY, value->GetType());

    DictionaryValue* d_value = static_cast<DictionaryValue*>(value);
    Value* roots;
    ASSERT_TRUE(d_value->Get(BookmarkCodec::kRootsKey, &roots));
    ASSERT_EQ(Value::TYPE_DICTIONARY, roots->GetType());

    DictionaryValue* roots_d_value = static_cast<DictionaryValue*>(roots);
    Value* bb_value;
    ASSERT_TRUE(roots_d_value->Get(BookmarkCodec::kRootFolderNameKey,
                                   &bb_value));
    ASSERT_EQ(Value::TYPE_DICTIONARY, bb_value->GetType());

    DictionaryValue* bb_d_value = static_cast<DictionaryValue*>(bb_value);
    Value* bb_children_value;
    ASSERT_TRUE(bb_d_value->Get(BookmarkCodec::kChildrenKey,
                                &bb_children_value));
    ASSERT_EQ(Value::TYPE_LIST, bb_children_value->GetType());

    ListValue* bb_children_l_value = static_cast<ListValue*>(bb_children_value);
    Value* child_value;
    ASSERT_TRUE(bb_children_l_value->Get(index, &child_value));
    ASSERT_EQ(Value::TYPE_DICTIONARY, child_value->GetType());

    *result_value = static_cast<DictionaryValue*>(child_value);
  }

  Value* EncodeHelper(BookmarkModel* model, std::string* checksum) {
    BookmarkCodec encoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", encoder.computed_checksum());
    EXPECT_EQ("", encoder.stored_checksum());

    scoped_ptr<Value> value(encoder.Encode(model));
    const std::string& computed_checksum = encoder.computed_checksum();
    const std::string& stored_checksum = encoder.stored_checksum();

    // Computed and stored checksums should not be empty and should be equal.
    EXPECT_FALSE(computed_checksum.empty());
    EXPECT_FALSE(stored_checksum.empty());
    EXPECT_EQ(computed_checksum, stored_checksum);

    *checksum = computed_checksum;
    return value.release();
  }

  bool Decode(BookmarkCodec* codec, BookmarkModel* model, const Value& value) {
    int64 max_id;
    bool result = codec->Decode(AsMutable(model->GetBookmarkBarNode()),
                                AsMutable(model->other_node()),
                                &max_id, value);
    model->set_next_node_id(max_id);
    return result;
  }

  BookmarkModel* DecodeHelper(const Value& value,
                              const std::string& expected_stored_checksum,
                              std::string* computed_checksum,
                              bool expected_changes) {
    BookmarkCodec decoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", decoder.computed_checksum());
    EXPECT_EQ("", decoder.stored_checksum());

    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    EXPECT_TRUE(Decode(&decoder, model.get(), value));

    *computed_checksum = decoder.computed_checksum();
    const std::string& stored_checksum = decoder.stored_checksum();

    // Computed and stored checksums should not be empty.
    EXPECT_FALSE(computed_checksum->empty());
    EXPECT_FALSE(stored_checksum.empty());

    // Stored checksum should be as expected.
    EXPECT_EQ(expected_stored_checksum, stored_checksum);

    // The two checksums should be equal if expected_changes is true; otherwise
    // they should be different.
    if (expected_changes)
      EXPECT_NE(*computed_checksum, stored_checksum);
    else
      EXPECT_EQ(*computed_checksum, stored_checksum);

    return model.release();
  }

  void CheckIDs(const BookmarkNode* node, std::set<int64>* assigned_ids) {
    DCHECK(node);
    int64 node_id = node->id();
    EXPECT_TRUE(assigned_ids->find(node_id) == assigned_ids->end());
    assigned_ids->insert(node_id);
    for (int i = 0; i < node->child_count(); ++i)
      CheckIDs(node->GetChild(i), assigned_ids);
  }

  void ExpectIDsUnique(BookmarkModel* model) {
    std::set<int64> assigned_ids;
    CheckIDs(model->GetBookmarkBarNode(), &assigned_ids);
    CheckIDs(model->other_node(), &assigned_ids);
  }
};

TEST_F(BookmarkCodecTest, ChecksumEncodeDecodeTest) {
  scoped_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  scoped_ptr<Value> value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  std::string dec_checksum;
  scoped_ptr<BookmarkModel> decoded_model(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, false));
}

TEST_F(BookmarkCodecTest, ChecksumEncodeIdenticalModelsTest) {
  // Encode two identical models and make sure the check-sums are same as long
  // as the data is the same.
  scoped_ptr<BookmarkModel> model1(CreateTestModel1());
  std::string enc_checksum1;
  scoped_ptr<Value> value1(EncodeHelper(model1.get(), &enc_checksum1));
  EXPECT_TRUE(value1.get() != NULL);

  scoped_ptr<BookmarkModel> model2(CreateTestModel1());
  std::string enc_checksum2;
  scoped_ptr<Value> value2(EncodeHelper(model2.get(), &enc_checksum2));
  EXPECT_TRUE(value2.get() != NULL);

  ASSERT_EQ(enc_checksum1, enc_checksum2);
}

TEST_F(BookmarkCodecTest, ChecksumManualEditTest) {
  scoped_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  scoped_ptr<Value> value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  // Change something in the encoded value before decoding it.
  DictionaryValue* child1_value;
  GetBookmarksBarChildValue(value.get(), 0, &child1_value);
  std::string title;
  ASSERT_TRUE(child1_value->GetString(BookmarkCodec::kNameKey, &title));
  child1_value->SetString(BookmarkCodec::kNameKey, title + "1");

  std::string dec_checksum;
  scoped_ptr<BookmarkModel> decoded_model1(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, true));

  // Undo the change and make sure the checksum is same as original.
  child1_value->SetString(BookmarkCodec::kNameKey, title);
  scoped_ptr<BookmarkModel> decoded_model2(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, false));
}

TEST_F(BookmarkCodecTest, ChecksumManualEditIDsTest) {
  scoped_ptr<BookmarkModel> model_to_encode(CreateTestModel3());

  // The test depends on existence of multiple children under bookmark bar, so
  // make sure that's the case.
  int bb_child_count = model_to_encode->GetBookmarkBarNode()->child_count();
  ASSERT_GT(bb_child_count, 1);

  std::string enc_checksum;
  scoped_ptr<Value> value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  // Change IDs for all children of bookmark bar to be 1.
  DictionaryValue* child_value;
  for (int i = 0; i < bb_child_count; ++i) {
    GetBookmarksBarChildValue(value.get(), i, &child_value);
    std::string id;
    ASSERT_TRUE(child_value->GetString(BookmarkCodec::kIdKey, &id));
    child_value->SetString(BookmarkCodec::kIdKey, "1");
  }

  std::string dec_checksum;
  scoped_ptr<BookmarkModel> decoded_model(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, true));

  ExpectIDsUnique(decoded_model.get());

  // add a few extra nodes to bookmark model and make sure IDs are still uniuqe.
  const BookmarkNode* bb_node = decoded_model->GetBookmarkBarNode();
  decoded_model->AddURL(bb_node, 0, ASCIIToUTF16("new url1"),
                        GURL("http://newurl1.com"));
  decoded_model->AddURL(bb_node, 0, ASCIIToUTF16("new url2"),
                        GURL("http://newurl2.com"));

  ExpectIDsUnique(decoded_model.get());
}

TEST_F(BookmarkCodecTest, PersistIDsTest) {
  scoped_ptr<BookmarkModel> model_to_encode(CreateTestModel3());
  BookmarkCodec encoder;
  scoped_ptr<Value> model_value(encoder.Encode(model_to_encode.get()));

  BookmarkModel decoded_model(NULL);
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, &decoded_model, *model_value.get()));
  BookmarkModelTestUtils::AssertModelsEqual(model_to_encode.get(),
                                            &decoded_model,
                                            true);

  // Add a couple of more items to the decoded bookmark model and make sure
  // ID persistence is working properly.
  const BookmarkNode* bookmark_bar = decoded_model.GetBookmarkBarNode();
  decoded_model.AddURL(
      bookmark_bar, bookmark_bar->child_count(), ASCIIToUTF16(kUrl3Title),
      GURL(kUrl3Url));
  const BookmarkNode* folder2_node = decoded_model.AddFolder(
      bookmark_bar, bookmark_bar->child_count(), ASCIIToUTF16(kFolder2Title));
  decoded_model.AddURL(folder2_node, 0, ASCIIToUTF16(kUrl4Title),
                       GURL(kUrl4Url));

  BookmarkCodec encoder2;
  scoped_ptr<Value> model_value2(encoder2.Encode(&decoded_model));

  BookmarkModel decoded_model2(NULL);
  BookmarkCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, &decoded_model2, *model_value2.get()));
  BookmarkModelTestUtils::AssertModelsEqual(&decoded_model,
                                            &decoded_model2,
                                            true);
}
