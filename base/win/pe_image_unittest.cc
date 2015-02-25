// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for PEImage.
#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "base/win/pe_image.h"
#include "base/win/windows_version.h"

namespace {

class Expectations {
 public:
  enum Value {
    SECTIONS = 0,
    IMPORTS_DLLS,
    DELAY_DLLS,
    EXPORTS,
    IMPORTS,
    DELAY_IMPORTS,
    RELOCS
  };

  enum Arch {
    ARCH_X86 = 0,
    ARCH_X64,
    ARCH_ALL
  };

  Expectations();

  void SetDefault(Value value, int count);
  void SetOverride(Value value, base::win::Version version,
                   Arch arch, int count);
  void SetOverride(Value value, base::win::Version version, int count);
  void SetOverride(Value value, Arch arch, int count);

  // returns -1 on failure.
  int GetExpectation(Value value);

 private:
  class Override {
   public:
    enum MatchType { MATCH_VERSION, MATCH_ARCH, MATCH_BOTH, MATCH_NONE };

    Override(Value value, base::win::Version version, Arch arch, int count)
      : value_(value), version_(version), arch_(arch), count_(count) {
    };

    bool Matches(Value value, base::win::Version version,
                 Arch arch, MatchType type) {
      if (value_ != value)
        return false;

      switch (type) {
        case MATCH_BOTH:
          return (arch == arch_ && version == version_);
        case MATCH_ARCH:
          return (arch == arch_ && version_ == base::win::VERSION_WIN_LAST);
        case MATCH_VERSION:
          return (arch_ == ARCH_ALL && version == version_);
        case MATCH_NONE:
          return (arch_ == ARCH_ALL && version_ == base::win::VERSION_WIN_LAST);
      }
      return false;
    }

    int GetCount() { return count_; }

   private:
    Value value_;
    base::win::Version version_;
    Arch arch_;
    int count_;
  };

  bool MatchesMyArch(Arch arch);

  std::vector<Override> overrides_;
  Arch my_arch_;
  base::win::Version my_version_;
};

Expectations::Expectations() {
  my_version_ = base::win::GetVersion();
#if defined(ARCH_CPU_64_BITS)
  my_arch_ = ARCH_X64;
#else
  my_arch_ = ARCH_X86;
#endif
}

int Expectations::GetExpectation(Value value) {
  // Prefer OS version specificity over Arch specificity.
  for (auto type : { Override::MATCH_BOTH,
                     Override::MATCH_VERSION,
                     Override::MATCH_ARCH,
                     Override::MATCH_NONE }) {
    for (auto override : overrides_) {
      if (override.Matches(value, my_version_, my_arch_, type))
        return override.GetCount();
    }
  }
  return -1;
}

void Expectations::SetDefault(Value value, int count) {
  SetOverride(value, base::win::VERSION_WIN_LAST, ARCH_ALL, count);
}

void Expectations::SetOverride(Value value,
                               base::win::Version version,
                               Arch arch,
                               int count) {
  overrides_.push_back(Override(value, version, arch, count));
}

void Expectations::SetOverride(Value value,
                               base::win::Version version,
                               int count) {
  SetOverride(value, version, ARCH_ALL, count);
}

void Expectations::SetOverride(Value value, Arch arch, int count) {
  SetOverride(value, base::win::VERSION_WIN_LAST, arch, count);
}

}  // namespace

namespace base {
namespace win {

namespace {

// Just counts the number of invocations.
bool ImportsCallback(const PEImage& image,
                     LPCSTR module,
                     DWORD ordinal,
                     LPCSTR name,
                     DWORD hint,
                     PIMAGE_THUNK_DATA iat,
                     PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool SectionsCallback(const PEImage& image,
                      PIMAGE_SECTION_HEADER header,
                      PVOID section_start,
                      DWORD section_size,
                      PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool RelocsCallback(const PEImage& image,
                    WORD type,
                    PVOID address,
                    PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool ImportChunksCallback(const PEImage& image,
                          LPCSTR module,
                          PIMAGE_THUNK_DATA name_table,
                          PIMAGE_THUNK_DATA iat,
                          PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool DelayImportChunksCallback(const PEImage& image,
                               PImgDelayDescr delay_descriptor,
                               LPCSTR module,
                               PIMAGE_THUNK_DATA name_table,
                               PIMAGE_THUNK_DATA iat,
                               PIMAGE_THUNK_DATA bound_iat,
                               PIMAGE_THUNK_DATA unload_iat,
                               PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool ExportsCallback(const PEImage& image,
                     DWORD ordinal,
                     DWORD hint,
                     LPCSTR name,
                     PVOID function,
                     LPCSTR forward,
                     PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

}  // namespace

// Tests that we are able to enumerate stuff from a PE file, and that
// the actual number of items found is within the expected range.
TEST(PEImageTest, EnumeratesPE) {
  Expectations expectations;

#ifndef NDEBUG
  // Default Debug expectations.
  expectations.SetDefault(Expectations::SECTIONS, 7);
  expectations.SetDefault(Expectations::IMPORTS_DLLS, 3);
  expectations.SetDefault(Expectations::DELAY_DLLS, 2);
  expectations.SetDefault(Expectations::EXPORTS, 2);
  expectations.SetDefault(Expectations::IMPORTS, 49);
  expectations.SetDefault(Expectations::DELAY_IMPORTS, 2);
  expectations.SetDefault(Expectations::RELOCS, 438);

  // 64-bit Debug expectations.
  expectations.SetOverride(Expectations::SECTIONS, Expectations::ARCH_X64, 8);
  expectations.SetOverride(Expectations::IMPORTS, Expectations::ARCH_X64, 69);
  expectations.SetOverride(Expectations::RELOCS, Expectations::ARCH_X64, 632);
#else
  // Default Release expectations.
  expectations.SetDefault(Expectations::SECTIONS, 5);
  expectations.SetDefault(Expectations::IMPORTS_DLLS, 2);
  expectations.SetDefault(Expectations::DELAY_DLLS, 2);
  expectations.SetDefault(Expectations::EXPORTS, 2);
  expectations.SetDefault(Expectations::IMPORTS, 66);
  expectations.SetDefault(Expectations::DELAY_IMPORTS, 2);
  expectations.SetDefault(Expectations::RELOCS, 1586);

  // 64-bit Release expectations.
  expectations.SetOverride(Expectations::SECTIONS, Expectations::ARCH_X64, 6);
  expectations.SetOverride(Expectations::IMPORTS, Expectations::ARCH_X64, 69);
  expectations.SetOverride(Expectations::RELOCS, Expectations::ARCH_X64, 632);
#endif

  HMODULE module = LoadLibrary(L"pe_image_test.dll");
  ASSERT_TRUE(NULL != module);

  PEImage pe(module);
  int count = 0;
  EXPECT_TRUE(pe.VerifyMagic());

  pe.EnumSections(SectionsCallback, &count);
  EXPECT_EQ(expectations.GetExpectation(Expectations::SECTIONS), count);

  count = 0;
  pe.EnumImportChunks(ImportChunksCallback, &count);
  EXPECT_EQ(expectations.GetExpectation(Expectations::IMPORTS_DLLS), count);

  count = 0;
  pe.EnumDelayImportChunks(DelayImportChunksCallback, &count);
  EXPECT_EQ(expectations.GetExpectation(Expectations::DELAY_DLLS), count);

  count = 0;
  pe.EnumExports(ExportsCallback, &count);
  EXPECT_EQ(expectations.GetExpectation(Expectations::EXPORTS), count);

  count = 0;
  pe.EnumAllImports(ImportsCallback, &count);
  EXPECT_EQ(expectations.GetExpectation(Expectations::IMPORTS), count);

  count = 0;
  pe.EnumAllDelayImports(ImportsCallback, &count);
  EXPECT_EQ(expectations.GetExpectation(Expectations::DELAY_IMPORTS), count);

  count = 0;
  pe.EnumRelocs(RelocsCallback, &count);
  EXPECT_EQ(expectations.GetExpectation(Expectations::RELOCS), count);

  FreeLibrary(module);
}

// Tests that we can locate an specific exported symbol, by name and by ordinal.
TEST(PEImageTest, RetrievesExports) {
  HMODULE module = LoadLibrary(L"advapi32.dll");
  ASSERT_TRUE(NULL != module);

  PEImage pe(module);
  WORD ordinal;

  EXPECT_TRUE(pe.GetProcOrdinal("RegEnumKeyExW", &ordinal));

  FARPROC address1 = pe.GetProcAddress("RegEnumKeyExW");
  FARPROC address2 = pe.GetProcAddress(reinterpret_cast<char*>(ordinal));
  EXPECT_TRUE(address1 != NULL);
  EXPECT_TRUE(address2 != NULL);
  EXPECT_TRUE(address1 == address2);

  FreeLibrary(module);
}

// Test that we can get debug id out of a module.
TEST(PEImageTest, GetDebugId) {
  HMODULE module = LoadLibrary(L"advapi32.dll");
  ASSERT_TRUE(NULL != module);

  PEImage pe(module);
  GUID guid = {0};
  DWORD age = 0;
  EXPECT_TRUE(pe.GetDebugId(&guid, &age));

  GUID empty_guid = {0};
  EXPECT_TRUE(!IsEqualGUID(empty_guid, guid));
  EXPECT_NE(0U, age);
  FreeLibrary(module);
}

}  // namespace win
}  // namespace base
