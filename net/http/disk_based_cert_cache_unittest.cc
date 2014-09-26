// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/disk_based_cert_cache.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/http/mock_http_cache.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Testing the DiskBasedCertCache requires constant use of the
// certificates in GetTestCertsDirectory(). The TestCertMetaData
// struct stores metadata relevant to the DiskBasedCertCache for
// each used test certificate.
struct TestCertMetaData {
  const char* file_name;
  const char* cache_key;
};

const TestCertMetaData kCert1 = {
    "root_ca_cert.pem", "cert:738D348A8AFCEC4F79C3E4B1845D985AF601AB0F"};

const TestCertMetaData kCert2 = {
    "ok_cert.pem", "cert:6C9DFD2CFA9885C71BE6DE0EA0CF962AC8F9131B"};

// MockTransactions are required to use the MockDiskCache backend.
// |key| is a cache key, and is equivalent to the key that will be
// used to store or retrieve certificates in the cache. |test_mode|
// is an integer that is used to indicate properties of the test
// transaction, mostly whether or not it is synchronous.
// For testing the DiskBasedCertCache, other data members of the struct
// are irrelevant. Only one MockTransaction per certificate can be used
// at a time.
MockTransaction CreateMockTransaction(const char* key, int test_mode) {
  MockTransaction transaction = {key,  "", base::Time(), "", LOAD_NORMAL,
                                 "",   "", base::Time(), "", test_mode,
                                 NULL, 0,  OK};

  return transaction;
}

// Helper class, for use with DiskBasedCertCache::GetCertificate, that will
// store the returned certificate handle and allow users to WaitForResult of
// DiskBasedCertCache::GetCertificate.
class TestGetCallback {
 public:
  TestGetCallback() : cert_handle_(NULL) {}
  ~TestGetCallback() {
    if (cert_handle_)
      X509Certificate::FreeOSCertHandle(cert_handle_);
  }

  // Blocks until the underlying GetCertificate() operation has succeeded.
  void WaitForResult() { cb_.WaitForResult(); }

  // Returns a Callback suitable for use with
  // DiskBasedCertCache::GetCertificate(). The returned callback is only valid
  // while the TestGetCallback object is still valid.
  DiskBasedCertCache::GetCallback callback() {
    return base::Bind(&TestGetCallback::OnGetComplete, base::Unretained(this));
  }

  // Returns the associated certificate handle.
  const X509Certificate::OSCertHandle& cert_handle() const {
    return cert_handle_;
  }

 private:
  void OnGetComplete(const X509Certificate::OSCertHandle handle) {
    if (handle)
      cert_handle_ = X509Certificate::DupOSCertHandle(handle);
    cb_.callback().Run(OK);
  }

  TestCompletionCallback cb_;
  X509Certificate::OSCertHandle cert_handle_;
};

// Helper class, for use with DiskBasedCertCache::SetCertificate, that will
// store the returned key and allow a user to WaitForResult of
// DiskBasedCertCache::SetCertificate.
class TestSetCallback {
 public:
  TestSetCallback() {}
  ~TestSetCallback() {}

  // Blocks until the underlying SetCertificate() operation has succeeded.
  void WaitForResult() { cb_.WaitForResult(); }

  // Returns a Callback suitable for use with
  // DiskBasedCertCache::SetCertificate(). The returned callback is only valid
  // while the TestSetCallback object is still  valid.
  DiskBasedCertCache::SetCallback callback() {
    return base::Bind(&TestSetCallback::OnSetComplete, base::Unretained(this));
  }

  // Returns the associated certificate handle.
  const std::string& key() const { return key_; }

 private:
  void OnSetComplete(const std::string& key) {
    key_ = key;
    cb_.callback().Run(OK);
  }

  TestCompletionCallback cb_;
  std::string key_;
};

// Stores the certificate corresponding to |cert_data| in |backend|. If
// |corrupt_data| is true, the certificate will be imported with errors
// so as to mimic a corrupted file on disk.
void ImportCert(disk_cache::Backend* backend,
                const TestCertMetaData& cert_data,
                bool corrupt_data) {
  disk_cache::Entry* entry;
  TestCompletionCallback callback;
  int rv =
      backend->CreateEntry(cert_data.cache_key, &entry, callback.callback());
  EXPECT_EQ(OK, callback.GetResult(rv));
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), cert_data.file_name));
  std::string write_data;
  bool encoded =
      X509Certificate::GetDEREncoded(cert->os_cert_handle(), &write_data);
  ASSERT_TRUE(encoded);
  if (corrupt_data) {
    for (size_t i = 0; i < write_data.size(); i += 20)
      ++write_data[i];
  }
  scoped_refptr<IOBuffer> buffer(new IOBuffer(write_data.size()));
  memcpy(buffer->data(), write_data.data(), write_data.size());
  rv = entry->WriteData(0 /* index */,
                        0 /* offset */,
                        buffer.get(),
                        write_data.size(),
                        callback.callback(),
                        true /* truncate */);
  ASSERT_EQ(static_cast<int>(write_data.size()), callback.GetResult(rv));
  entry->Close();
}

// Checks that the the certificate corresponding to |cert_data| is an existing,
// correctly cached entry in |backend|.
void CheckCertCached(disk_cache::Backend* backend,
                     const TestCertMetaData& cert_data) {
  disk_cache::Entry* entry;
  TestCompletionCallback callback;
  int rv = backend->OpenEntry(cert_data.cache_key, &entry, callback.callback());
  EXPECT_EQ(OK, callback.GetResult(rv));
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), cert_data.file_name));
  std::string write_data;
  bool encoded =
      X509Certificate::GetDEREncoded(cert->os_cert_handle(), &write_data);
  ASSERT_TRUE(encoded);
  int entry_size = entry->GetDataSize(0 /* index */);
  scoped_refptr<IOBuffer> buffer(new IOBuffer(entry_size));
  rv = entry->ReadData(0 /* index */,
                       0 /* offset */,
                       buffer.get(),
                       entry_size,
                       callback.callback());
  EXPECT_EQ(entry_size, callback.GetResult(rv));
  entry->Close();
  X509Certificate::OSCertHandle cached_cert_handle =
      X509Certificate::CreateOSCertHandleFromBytes(buffer->data(), entry_size);
  EXPECT_TRUE(X509Certificate::IsSameOSCert(cached_cert_handle,
                                            cert->os_cert_handle()));
  X509Certificate::FreeOSCertHandle(cached_cert_handle);
}

}  // namespace

// ----------------------------------------------------------------------------

// Tests that a certificate can be stored in the cache.
TEST(DiskBasedCertCache, SetCert) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  DiskBasedCertCache cache(&backend);
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  ASSERT_TRUE(cert.get());
  TestSetCallback set_callback;

  cache.SetCertificate(cert->os_cert_handle(), set_callback.callback());
  set_callback.WaitForResult();
  EXPECT_EQ(kCert1.cache_key, set_callback.key());
  ASSERT_NO_FATAL_FAILURE(CheckCertCached(&backend, kCert1));
}

// Tests that a certificate can be retrieved from the cache.
TEST(DiskBasedCertCache, GetCert) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  ASSERT_NO_FATAL_FAILURE(
      ImportCert(&backend, kCert1, false /* not corrupted */));
  DiskBasedCertCache cache(&backend);
  TestGetCallback get_callback;

  cache.GetCertificate(kCert1.cache_key, get_callback.callback());
  get_callback.WaitForResult();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(get_callback.cert_handle(),
                                            cert->os_cert_handle()));
}

// Tests that the DiskBasedCertCache successfully writes to the cache
// if the cache acts synchronously
TEST(DiskBasedCertCache, SyncSet) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_SYNC_ALL));
  MockDiskCache backend;
  DiskBasedCertCache cache(&backend);
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  ASSERT_TRUE(cert.get());

  TestSetCallback set_callback;
  cache.SetCertificate(cert->os_cert_handle(), set_callback.callback());
  set_callback.WaitForResult();
  EXPECT_EQ(kCert1.cache_key, set_callback.key());
  ASSERT_NO_FATAL_FAILURE(CheckCertCached(&backend, kCert1));
}

// Tests that the DiskBasedCertCache successfully reads from the cache
// if the cache acts synchronously
TEST(DiskBasedCertCache, SyncGet) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_SYNC_ALL));
  MockDiskCache backend;
  ASSERT_NO_FATAL_FAILURE(
      (ImportCert(&backend, kCert1, false /* not corrupted */)));
  DiskBasedCertCache cache(&backend);
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  ASSERT_TRUE(cert.get());

  TestGetCallback get_callback;
  cache.GetCertificate(kCert1.cache_key, get_callback.callback());
  get_callback.WaitForResult();
  EXPECT_TRUE(X509Certificate::IsSameOSCert(get_callback.cert_handle(),
                                            cert->os_cert_handle()));
}

// Tests that GetCertificate will fail on a corrupted certificate.
TEST(DiskBasedCertCache, GetBrokenCert) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  ASSERT_NO_FATAL_FAILURE(ImportCert(&backend, kCert1, true /* corrupted */));
  DiskBasedCertCache cache(&backend);
  TestGetCallback get_callback;

  cache.GetCertificate(kCert1.cache_key, get_callback.callback());
  get_callback.WaitForResult();

  EXPECT_FALSE(get_callback.cert_handle());
}

// Tests that attempting to retrieve a cert that is not in the cache will
// return NULL.
TEST(DiskBasedCertCache, GetUncachedCert) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  DiskBasedCertCache cache(&backend);
  TestGetCallback get_callback;

  cache.GetCertificate(kCert1.cache_key, get_callback.callback());
  get_callback.WaitForResult();
  EXPECT_EQ(NULL, get_callback.cert_handle());
}

// Issues two requests to store a certificate in the cache
// (simultaneously), and checks that the DiskBasedCertCache stores the
// certificate to the cache (in one write rather than two).
TEST(DiskBasedCertCache, SetMultiple) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  DiskBasedCertCache cache(&backend);
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  ASSERT_TRUE(cert.get());
  TestSetCallback set_callback1, set_callback2;

  // Behind the scenes, these two operations will be combined
  // into one operation. IgnoreCallbacks guarantees that the
  // first SetCertificate operation is not yet complete when the second
  // SetCertificate is called, and then IgnoreCallbacks(false) continues the
  // (combined) operation in the |cache|.
  MockDiskEntry::IgnoreCallbacks(true);
  cache.SetCertificate(cert->os_cert_handle(), set_callback1.callback());
  cache.SetCertificate(cert->os_cert_handle(), set_callback2.callback());
  MockDiskEntry::IgnoreCallbacks(false);

  set_callback1.WaitForResult();
  set_callback2.WaitForResult();
  EXPECT_EQ(set_callback1.key(), set_callback2.key());
  ASSERT_NO_FATAL_FAILURE(CheckCertCached(&backend, kCert1));
}

// Issues two requests to store a certificate in the cache
// because the first transaction finishes before the second
// one is issued, the first cache write is overwritten.
TEST(DiskBasedCertCache, SetOverwrite) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  backend.set_double_create_check(false);
  DiskBasedCertCache cache(&backend);
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  ASSERT_TRUE(cert.get());
  TestSetCallback set_callback1, set_callback2;

  cache.SetCertificate(cert->os_cert_handle(), set_callback1.callback());
  set_callback1.WaitForResult();
  cache.SetCertificate(cert->os_cert_handle(), set_callback2.callback());
  set_callback2.WaitForResult();

  EXPECT_EQ(set_callback1.key(), set_callback2.key());
  ASSERT_NO_FATAL_FAILURE(CheckCertCached(&backend, kCert1));
}

// Stores a certificate in the DiskBasedCertCache, then retrieves it
// and makes sure it was retrieved successfully.
TEST(DiskBasedCertCache, SimpleSetAndGet) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  DiskBasedCertCache cache(&backend);
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  ASSERT_TRUE(cert.get());
  TestSetCallback set_callback;
  TestGetCallback get_callback;

  cache.SetCertificate(cert->os_cert_handle(), set_callback.callback());
  set_callback.WaitForResult();
  cache.GetCertificate(set_callback.key(), get_callback.callback());
  get_callback.WaitForResult();
  EXPECT_TRUE(X509Certificate::IsSameOSCert(get_callback.cert_handle(),
                                            cert->os_cert_handle()));
}

// Tests some basic functionality of the DiskBasedCertCache, with multiple
// set and get operations.
TEST(DiskBasedCertCache, BasicUsage) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_SYNC_CACHE_START));
  ScopedMockTransaction trans2(
      CreateMockTransaction(kCert2.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  DiskBasedCertCache cache(&backend);
  scoped_refptr<X509Certificate> cert1(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  scoped_refptr<X509Certificate> cert2(
      ImportCertFromFile(GetTestCertsDirectory(), kCert2.file_name));
  ASSERT_TRUE(cert1.get());
  ASSERT_TRUE(cert2.get());
  ASSERT_FALSE(X509Certificate::IsSameOSCert(cert1->os_cert_handle(),
                                             cert2->os_cert_handle()));
  TestSetCallback set_callback1, set_callback2;

  // Callbacks are temporarily ignored here to guarantee the asynchronous
  // operations of the DiskBasedCertCache are always executed in the same
  // order.
  MockDiskEntry::IgnoreCallbacks(true);
  cache.SetCertificate(cert1->os_cert_handle(), set_callback1.callback());
  cache.SetCertificate(cert2->os_cert_handle(), set_callback2.callback());
  MockDiskEntry::IgnoreCallbacks(false);
  set_callback1.WaitForResult();
  set_callback2.WaitForResult();

  TestGetCallback get_callback1, get_callback2;

  MockDiskEntry::IgnoreCallbacks(true);
  cache.GetCertificate(set_callback1.key(), get_callback1.callback());
  cache.GetCertificate(set_callback2.key(), get_callback2.callback());
  MockDiskEntry::IgnoreCallbacks(false);
  get_callback1.WaitForResult();
  get_callback2.WaitForResult();

  EXPECT_TRUE(X509Certificate::IsSameOSCert(cert1->os_cert_handle(),
                                            get_callback1.cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(cert2->os_cert_handle(),
                                            get_callback2.cert_handle()));
}

// Test the result of simultaneous requests to store and retrieve a
// certificate from the cache, with the get operation attempting to
// open the cache first and therefore failing to open the entry.
TEST(DiskBasedCertCache, SimultaneousGetSet) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_SYNC_CACHE_START));
  MockDiskCache backend;
  DiskBasedCertCache cache(&backend);
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  ASSERT_TRUE(cert.get());

  TestGetCallback get_callback;
  TestSetCallback set_callback;

  MockDiskEntry::IgnoreCallbacks(true);
  cache.GetCertificate(kCert1.cache_key, get_callback.callback());
  cache.SetCertificate(cert->os_cert_handle(), set_callback.callback());
  MockDiskEntry::IgnoreCallbacks(false);
  get_callback.WaitForResult();
  set_callback.WaitForResult();

  EXPECT_EQ(NULL, get_callback.cert_handle());
  EXPECT_EQ(kCert1.cache_key, set_callback.key());
}

// Test the result of simultaneous requests to store and retrieve a
// certificate from the cache, with the get operation opening the cache
// after the set operation, leading to a successful read.
TEST(DiskBasedCertCache, SimultaneousSetGet) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_SYNC_CACHE_START));
  MockDiskCache backend;
  DiskBasedCertCache cache(&backend);
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  ASSERT_TRUE(cert.get());

  TestSetCallback set_callback;
  TestGetCallback get_callback;

  MockDiskEntry::IgnoreCallbacks(true);
  cache.SetCertificate(cert->os_cert_handle(), set_callback.callback());
  cache.GetCertificate(kCert1.cache_key, get_callback.callback());
  MockDiskEntry::IgnoreCallbacks(false);
  set_callback.WaitForResult();
  get_callback.WaitForResult();

  EXPECT_EQ(kCert1.cache_key, set_callback.key());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(cert->os_cert_handle(),
                                            get_callback.cert_handle()));
}

// Tests that the DiskBasedCertCache can be deleted without issues when
// there are pending operations in the disk cache.
TEST(DiskBasedCertCache, DeletedCertCache) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  scoped_ptr<DiskBasedCertCache> cache(new DiskBasedCertCache(&backend));
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  ASSERT_TRUE(cert.get());
  TestSetCallback set_callback;

  cache->SetCertificate(cert->os_cert_handle(), set_callback.callback());
  cache.reset();
  set_callback.WaitForResult();
  EXPECT_EQ(std::string(), set_callback.key());
}

// Issues two successive read requests for a certificate, and then
// checks that the DiskBasedCertCache correctly read and recorded
// reading through the in-memory MRU cache.
TEST(DiskBasedCertCache, MemCacheGet) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  ASSERT_NO_FATAL_FAILURE(
      ImportCert(&backend, kCert1, false /* not corrupted */));
  DiskBasedCertCache cache(&backend);

  TestGetCallback get_callback1, get_callback2;
  cache.GetCertificate(kCert1.cache_key, get_callback1.callback());
  get_callback1.WaitForResult();
  EXPECT_EQ(0U, cache.mem_cache_hits_for_testing());
  cache.GetCertificate(kCert1.cache_key, get_callback2.callback());
  get_callback2.WaitForResult();
  EXPECT_EQ(1U, cache.mem_cache_hits_for_testing());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(get_callback1.cert_handle(),
                                            get_callback2.cert_handle()));
}

// Reads a corrupted certificate from the disk cache, and then overwrites
// it and checks that the uncorrupted version was stored in the in-memory
// cache.
TEST(DiskBasedCertCache, CorruptOverwrite) {
  ScopedMockTransaction trans1(
      CreateMockTransaction(kCert1.cache_key, TEST_MODE_NORMAL));
  MockDiskCache backend;
  backend.set_double_create_check(false);
  ASSERT_NO_FATAL_FAILURE(ImportCert(&backend, kCert1, true /* corrupted */));
  DiskBasedCertCache cache(&backend);
  TestGetCallback get_callback1, get_callback2;

  cache.GetCertificate(kCert1.cache_key, get_callback1.callback());
  get_callback1.WaitForResult();
  EXPECT_FALSE(get_callback2.cert_handle());

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), kCert1.file_name));
  TestSetCallback set_callback;

  cache.SetCertificate(cert->os_cert_handle(), set_callback.callback());
  set_callback.WaitForResult();
  EXPECT_EQ(kCert1.cache_key, set_callback.key());
  EXPECT_EQ(0U, cache.mem_cache_hits_for_testing());

  cache.GetCertificate(kCert1.cache_key, get_callback2.callback());
  get_callback2.WaitForResult();
  EXPECT_TRUE(X509Certificate::IsSameOSCert(get_callback2.cert_handle(),
                                            cert->os_cert_handle()));
  EXPECT_EQ(1U, cache.mem_cache_hits_for_testing());
  ASSERT_NO_FATAL_FAILURE(CheckCertCached(&backend, kCert1));
}

}  // namespace net
