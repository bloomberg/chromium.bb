// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CEEE_COMMON_NP_BROWSER_FUNCTIONS_H_
#define CEEE_COMMON_NP_BROWSER_FUNCTIONS_H_

#include <string>
#include "base/logging.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace npapi {

// Must be called prior to calling any of the browser functions below.
void InitializeBrowserFunctions(NPNetscapeFuncs* functions);
void UninitializeBrowserFunctions();

// Returns true iff InitializeBrowserFunctions has been called successully.
bool IsInitialized();

// Function stubs for functions that the host browser implements.

NPError GetURL(NPP instance, const char* URL, const char* window);

NPError PostURL(NPP instance, const char* URL, const char* window, uint32 len,
                const char* buf, NPBool file);

NPError RequestRead(NPStream* stream, NPByteRange* rangeList);

NPError NewStream(NPP instance, NPMIMEType type, const char* window,
                  NPStream** stream);

int32 Write(NPP instance, NPStream* stream, int32 len, void* buffer);

NPError DestroyStream(NPP instance, NPStream* stream, NPReason reason);

void Status(NPP instance, const char* message);

const char* UserAgent(NPP instance);

void* MemAlloc(uint32 size);

void MemFree(void* ptr);

uint32 MemFlush(uint32 size);

void ReloadPlugins(NPBool reloadPages);

void* GetJavaEnv();

void* GetJavaPeer(NPP instance);

NPError GetURLNotify(NPP instance, const char* URL, const char* window,
                     void* notifyData);

NPError PostURLNotify(NPP instance, const char* URL, const char* window,
                      uint32 len, const char* buf, NPBool file,
                      void* notifyData);

NPError GetValue(NPP instance, NPNVariable variable, void* ret_value);

NPError SetValue(NPP instance, NPPVariable variable, void* value);

void InvalidateRect(NPP instance, NPRect* rect);

void InvalidateRegion(NPP instance, NPRegion region);

void ForceRedraw(NPP instance);

void ReleaseVariantValue(NPVariant* variant);

NPIdentifier GetStringIdentifier(const NPUTF8* name);

void GetStringIdentifiers(const NPUTF8** names, int32_t nameCount,
                          NPIdentifier* identifiers);

NPIdentifier GetIntIdentifier(int32_t intid);

int32_t IntFromIdentifier(NPIdentifier identifier);

bool IdentifierIsString(NPIdentifier identifier);

NPUTF8* UTF8FromIdentifier(NPIdentifier identifier);

NPObject* CreateObject(NPP, NPClass* aClass);

NPObject* RetainObject(NPObject* obj);

void ReleaseObject(NPObject* obj);

bool Invoke(NPP npp, NPObject* obj, NPIdentifier methodName,
            const NPVariant* args, unsigned argCount, NPVariant* result);

bool InvokeDefault(NPP npp, NPObject* obj, const NPVariant* args,
                   unsigned argCount, NPVariant* result);

bool Evaluate(NPP npp, NPObject* obj, NPString* script, NPVariant* result);

bool GetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                 NPVariant* result);

bool SetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                 const NPVariant* value);

bool HasProperty(NPP npp, NPObject* npobj, NPIdentifier propertyName);

bool HasMethod(NPP npp, NPObject* npobj, NPIdentifier methodName);

bool RemoveProperty(NPP npp, NPObject* obj, NPIdentifier propertyName);

void SetException(NPObject* obj, const NPUTF8* message);

void PushPopupsEnabledState(NPP npp, NPBool enabled);

void PopPopupsEnabledState(NPP npp);

bool Enumerate(NPP npp, NPObject* obj, NPIdentifier** identifier,
               uint32_t* count);

void PluginThreadAsyncCall(NPP instance,
                           void (*func)(void*),  // NOLINT
                           void* userData);

bool Construct(NPP npp, NPObject* obj, const NPVariant* args, uint32_t argCount,
               NPVariant* result);

// Helper routine that wraps UTF8FromIdentifier to convert a string identifier
// to an STL string.  It's not super efficient since it could possibly do two
// heap allocations (STL string has a stack based buffer for smaller strings).
// For debugging purposes it is useful.
std::string StringFromIdentifier(NPIdentifier identifier);

}  // namespace npapi

// Simple helper class for freeing NPVariants at the end of a scope.
class ScopedNpVariant : public NPVariant {
 public:
  ScopedNpVariant() {
    VOID_TO_NPVARIANT(*this);
  }

  ~ScopedNpVariant() {
    Free();
  }

  void Free() {
    npapi::ReleaseVariantValue(this);
    VOID_TO_NPVARIANT(*this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedNpVariant);
};

// Simple helper class for freeing NPObjects at the end of a scope.
template <typename NpoType = NPObject>
class ScopedNpObject {
 public:
  ScopedNpObject() : npo_(NULL) {
  }

  explicit ScopedNpObject(NpoType* npo) : npo_(npo) {
  }

  ~ScopedNpObject() {
    Free();
  }

  NpoType* get() const {
    return npo_;
  }

  operator NpoType*() const {
    return npo_;
  }

  NpoType* operator->() const {
    return npo_;
  }

  ScopedNpObject<NpoType>& operator=(NpoType* npo) {
    if (npo != npo_) {
      DCHECK(npo_ == NULL);
      npapi::RetainObject(npo);
      npo_ = npo;
    }
    return *this;
  }

  void Free() {
    if (npo_) {
      npapi::ReleaseObject(npo_);
      npo_ = NULL;
    }
  }

  NpoType** Receive() {
    DCHECK(npo_ == NULL) << "Object leak. Pointer must be NULL";
    return &npo_;
  }

  NpoType* Detach() {
    NpoType* p = npo_;
    npo_ = NULL;
    return p;
  }

  void Attach(NpoType* p) {
    DCHECK(npo_ == NULL);
    npo_ = p;
  }

  NpoType* Copy() const {
    if (npo_ != NULL)
      npapi::RetainObject(npo_);
    return npo_;
  }

  bool Invoke0(NPP npp, NPIdentifier id, NPVariant* result) {
    return npapi::Invoke(npp, npo_, id, NULL, 0, result);
  }
  bool Invoke0(NPP npp, const NPUTF8* name, NPVariant* result) {
    return Invoke0(npp, npapi::GetStringIdentifier(name), result);
  }

  bool Invoke1(NPP npp, NPIdentifier id, const NPVariant &arg1,
               NPVariant* result) {
    return npapi::Invoke(npp, npo_, id, &arg1, 1, result);
  }
  bool Invoke1(NPP npp, const NPUTF8* name, const NPVariant &arg1,
               NPVariant* result) {
    return Invoke1(npp, npapi::GetStringIdentifier(name), arg1, result);
  }
  bool InvokeN(NPP npp, NPIdentifier id, const NPVariant* args, unsigned argc,
               NPVariant* result) {
    return npapi::Invoke(npp, npo_, id, args, argc, result);
  }
  bool InvokeN(NPP npp, const NPUTF8* name, const NPVariant* args,
               unsigned argc, NPVariant* result) {
    return Invoke1(npp, npapi::GetStringIdentifier(name), args, argc, result);
  }

  bool GetProperty(NPP npp, NPIdentifier id, NPVariant* result) {
    return npapi::GetProperty(npp, npo_, id, result);
  }

  bool GetProperty(NPP npp, const NPUTF8* name, NPVariant* result) {
    return GetProperty(npp, npapi::GetStringIdentifier(name), result);
  }

 private:
  NpoType* npo_;
  DISALLOW_COPY_AND_ASSIGN(ScopedNpObject);
};

// Allocates a new NPUTF8 string and assigns it to the variant.
// If memory allocation fails, the variant type will be set to NULL.
// The memory allocation is done via the npapi browser functions.
void AllocateStringVariant(const std::string& str, NPVariant* var);

#endif  // CEEE_COMMON_NP_BROWSER_FUNCTIONS_H_
