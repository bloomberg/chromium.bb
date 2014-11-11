// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/common/cld_data_source.h"

#include "base/lazy_instance.h"

namespace {

// This is the global instance managed by Get/Set/SetDefault
translate::CldDataSource* g_instance = NULL;

// Global instances for the builtin data source types
base::LazyInstance<translate::CldDataSource>::Leaky g_wrapped_static =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<translate::CldDataSource>::Leaky g_wrapped_standalone =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<translate::CldDataSource>::Leaky g_wrapped_component =
    LAZY_INSTANCE_INITIALIZER;

bool g_disable_sanity_checks_for_test = false;

}  // namespace

namespace translate {

CldDataSource::CldDataSource() : m_cached_filepath(), m_file_lock() {
}

std::string CldDataSource::GetName() {
  if (this == GetStaticDataSource()) return "static";
  if (this == GetComponentDataSource()) return "component";
  if (this == GetStandaloneDataSource()) return "standalone";
  NOTREACHED() << "Subclass failed to override GetName()";
  return "unknown";
}

// static
void CldDataSource::DisableSanityChecksForTest() {
  g_disable_sanity_checks_for_test = true;
}

// static
void CldDataSource::EnableSanityChecksForTest() {
  g_disable_sanity_checks_for_test = false;
}

void CldDataSource::SetCldDataFilePath(const base::FilePath& path) {
  DCHECK(g_disable_sanity_checks_for_test ||
         this == GetComponentDataSource() ||
         this == GetStandaloneDataSource()) << "CLD Data Source misconfigured!";
  base::AutoLock lock(m_file_lock);
  m_cached_filepath = path;
}

base::FilePath CldDataSource::GetCldDataFilePath() {
  DCHECK(g_disable_sanity_checks_for_test ||
         this == GetComponentDataSource() ||
         this == GetStandaloneDataSource()) << "CLD Data Source misconfigured!";
  base::AutoLock lock(m_file_lock);
  return m_cached_filepath;
}

// static
void CldDataSource::SetDefault(CldDataSource* data_source) {
  if (g_instance == NULL) Set(data_source);
}

// static
void CldDataSource::Set(CldDataSource* data_source) {
  g_instance = data_source;
}

// static
CldDataSource* CldDataSource::Get() {
  DCHECK(g_instance != NULL) << "No CLD data source was configured!";
  if (g_instance != NULL) return g_instance;
  return GetStaticDataSource();
}

// static
CldDataSource* CldDataSource::GetStaticDataSource() {
  return &g_wrapped_static.Get();
}

// static
bool CldDataSource::IsUsingStaticDataSource() {
  return Get()->GetName() == "static";
}

// static
CldDataSource* CldDataSource::GetStandaloneDataSource() {
  return &g_wrapped_standalone.Get();
}

// static
bool CldDataSource::IsUsingStandaloneDataSource() {
  return Get()->GetName() == "standalone";
}

// static
CldDataSource* CldDataSource::GetComponentDataSource() {
  return &g_wrapped_component.Get();
}

// static
bool CldDataSource::IsUsingComponentDataSource() {
  return Get()->GetName() == "component";
}

}  // namespace translate
