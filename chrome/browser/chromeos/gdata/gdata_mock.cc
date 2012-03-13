// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_mock.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;

namespace gdata {

namespace {

static Value* LoadJSONFile(const std::string& filename) {
  FilePath path;
  std::string error;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("chromeos")
      .AppendASCII("gdata")
      .AppendASCII(filename.c_str());
  EXPECT_TRUE(file_util::PathExists(path)) <<
      "Couldn't find " << path.value();

  JSONFileValueSerializer serializer(path);
  Value* value = serializer.Deserialize(NULL, &error);
  EXPECT_TRUE(value) <<
      "Parse error " << path.value() << ": " << error;
  return value;
}

}  // namespace

MockDocumentsService::MockDocumentsService() {
  ON_CALL(*this, Authenticate(_))
      .WillByDefault(Invoke(this, &MockDocumentsService::AuthenticateStub));
  ON_CALL(*this, GetDocuments(_, _))
      .WillByDefault(Invoke(this, &MockDocumentsService::GetDocumentsStub));
  ON_CALL(*this, DeleteDocument(_, _))
      .WillByDefault(Invoke(this, &MockDocumentsService::DeleteDocumentStub));
  ON_CALL(*this, DownloadDocument(_, _, _))
      .WillByDefault(Invoke(this, &MockDocumentsService::DownloadDocumentStub));
  ON_CALL(*this, CopyDocument(_, _, _))
      .WillByDefault(Invoke(this, &MockDocumentsService::CopyDocumentStub));
  ON_CALL(*this, RenameResource(_, _, _))
      .WillByDefault(Invoke(this, &MockDocumentsService::RenameResourceStub));
  ON_CALL(*this, AddResourceToDirectory(_, _, _))
      .WillByDefault(
          Invoke(this, &MockDocumentsService::AddResourceToDirectoryStub));
  ON_CALL(*this, RemoveResourceFromDirectory(_, _, _, _))
      .WillByDefault(
          Invoke(this, &MockDocumentsService::RemoveResourceFromDirectoryStub));
  ON_CALL(*this, CreateDirectory(_, _, _))
      .WillByDefault(Invoke(this, &MockDocumentsService::CreateDirectoryStub));
  ON_CALL(*this, DownloadFile(_, _))
      .WillByDefault(Invoke(this, &MockDocumentsService::DownloadFileStub));

  // Fill in the default values for mock feeds.
  feed_data_.reset(LoadJSONFile("basic_feed.json"));
  directory_data_.reset(LoadJSONFile("subdir_feed.json"));
}

MockDocumentsService::~MockDocumentsService() {}

void MockDocumentsService::AuthenticateStub(
    const AuthStatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, "my_auth_token"));
}

void MockDocumentsService::GetDocumentsStub(
    const GURL& feed_url,
    const GetDataCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&feed_data_)));
}

void MockDocumentsService::DeleteDocumentStub(
    const GURL& document_url,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, document_url));
}

void MockDocumentsService::DownloadDocumentStub(
    const GURL& content_url,
    DocumentExportFormat format,
    const DownloadActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, content_url,
                 FilePath(content_url.host() + content_url.path())));
}

void MockDocumentsService::CopyDocumentStub(
    const GURL& document_url,
    const FilePath::StringType& new_name,
    const GetDataCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&document_data_)));
}

void MockDocumentsService::RenameResourceStub(
    const GURL& resource_url,
    const FilePath::StringType& new_name,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, resource_url));
}

void MockDocumentsService::AddResourceToDirectoryStub(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, resource_url));
}

void MockDocumentsService::RemoveResourceFromDirectoryStub(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, resource_url));
}

void MockDocumentsService::CreateDirectoryStub(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const GetDataCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&directory_data_)));
}

void MockDocumentsService::DownloadFileStub(
    const GURL& content_url,
    const DownloadActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, content_url, FilePath(
          content_url.host() + content_url.path())));
}

}  // namespace gdata
