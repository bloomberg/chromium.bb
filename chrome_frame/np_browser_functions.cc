// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/np_browser_functions.h"

#include "base/logging.h"

namespace npapi {

// global function pointers (within this namespace) for the NPN functions.
struct NpVersion {
  struct {
    uint8 minor;
    uint8 major;
  } v;

  void set_version(uint16 version) {
    v.minor = version & 0xFF;
    v.major = version >> 8;
  }

  uint16 version() const {
    return v.minor | (v.major << 8);
  }
};

NpVersion g_version = {0};

NPN_GetURLProcPtr g_geturl = NULL;
NPN_PostURLProcPtr g_posturl = NULL;
NPN_RequestReadProcPtr g_requestread = NULL;
NPN_NewStreamProcPtr g_newstream = NULL;
NPN_WriteProcPtr g_write = NULL;
NPN_DestroyStreamProcPtr g_destroystream = NULL;
NPN_StatusProcPtr g_status = NULL;
NPN_UserAgentProcPtr g_useragent = NULL;
NPN_MemAllocProcPtr g_memalloc = NULL;
NPN_MemFreeProcPtr g_memfree = NULL;
NPN_MemFlushProcPtr g_memflush = NULL;
NPN_ReloadPluginsProcPtr g_reloadplugins = NULL;
NPN_GetJavaEnvProcPtr g_getJavaEnv = NULL;
NPN_GetJavaPeerProcPtr g_getJavaPeer = NULL;
NPN_GetURLNotifyProcPtr g_geturlnotify = NULL;
NPN_PostURLNotifyProcPtr g_posturlnotify = NULL;
NPN_GetValueProcPtr g_getvalue = NULL;
NPN_SetValueProcPtr g_setvalue = NULL;
NPN_InvalidateRectProcPtr g_invalidaterect = NULL;
NPN_InvalidateRegionProcPtr g_invalidateregion = NULL;
NPN_ForceRedrawProcPtr g_forceredraw = NULL;
NPN_GetStringIdentifierProcPtr g_getstringidentifier = NULL;
NPN_GetStringIdentifiersProcPtr g_getstringidentifiers = NULL;
NPN_GetIntIdentifierProcPtr g_getintidentifier = NULL;
NPN_IdentifierIsStringProcPtr g_identifierisstring = NULL;
NPN_UTF8FromIdentifierProcPtr g_utf8fromidentifier = NULL;
NPN_IntFromIdentifierProcPtr g_intfromidentifier = NULL;
NPN_CreateObjectProcPtr g_createobject = NULL;
NPN_RetainObjectProcPtr g_retainobject = NULL;
NPN_ReleaseObjectProcPtr g_releaseobject = NULL;
NPN_InvokeProcPtr g_invoke = NULL;
NPN_InvokeDefaultProcPtr g_invoke_default = NULL;
NPN_EvaluateProcPtr g_evaluate = NULL;
NPN_GetPropertyProcPtr g_getproperty = NULL;
NPN_SetPropertyProcPtr g_setproperty = NULL;
NPN_RemovePropertyProcPtr g_removeproperty = NULL;
NPN_HasPropertyProcPtr g_hasproperty = NULL;
NPN_HasMethodProcPtr g_hasmethod = NULL;
NPN_ReleaseVariantValueProcPtr g_releasevariantvalue = NULL;
NPN_SetExceptionProcPtr g_setexception = NULL;
NPN_PushPopupsEnabledStateProcPtr g_pushpopupsenabledstate = NULL;
NPN_PopPopupsEnabledStateProcPtr g_poppopupsenabledstate = NULL;
NPN_EnumerateProcPtr g_enumerate = NULL;
NPN_PluginThreadAsyncCallProcPtr g_pluginthreadasynccall = NULL;
NPN_ConstructProcPtr g_construct = NULL;
NPN_GetValueForURLPtr g_getvalueforurl = NULL;
NPN_SetValueForURLPtr g_setvalueforurl = NULL;
NPN_GetAuthenticationInfoPtr g_getauthenticationinfo = NULL;
NPN_URLRedirectResponsePtr g_urlredirectresponse = NULL;

// Must be called prior to calling any of the browser functions below.
void InitializeBrowserFunctions(NPNetscapeFuncs* functions) {
  CHECK(functions);
  DCHECK(g_geturl == NULL || g_geturl == functions->geturl);

  g_version.set_version(functions->version);

  g_geturl = functions->geturl;
  g_posturl = functions->posturl;
  g_requestread = functions->requestread;
  g_newstream = functions->newstream;
  g_write = functions->write;
  g_destroystream = functions->destroystream;
  g_status = functions->status;
  g_useragent = functions->uagent;
  g_memalloc = functions->memalloc;
  g_memfree = functions->memfree;
  g_memflush = functions->memflush;
  g_reloadplugins = functions->reloadplugins;
  g_getJavaEnv = functions->getJavaEnv;
  g_getJavaPeer = functions->getJavaPeer;
  g_geturlnotify = functions->geturlnotify;
  g_posturlnotify = functions->posturlnotify;
  g_getvalue = functions->getvalue;
  g_setvalue = functions->setvalue;
  g_invalidaterect = functions->invalidaterect;
  g_invalidateregion = functions->invalidateregion;
  g_forceredraw = functions->forceredraw;
  g_getstringidentifier = functions->getstringidentifier;
  g_getstringidentifiers = functions->getstringidentifiers;
  g_getintidentifier = functions->getintidentifier;
  g_identifierisstring = functions->identifierisstring;
  g_utf8fromidentifier = functions->utf8fromidentifier;
  g_intfromidentifier = functions->intfromidentifier;
  g_createobject = functions->createobject;
  g_retainobject = functions->retainobject;
  g_releaseobject = functions->releaseobject;
  g_invoke = functions->invoke;
  g_invoke_default = functions->invokeDefault;
  g_evaluate = functions->evaluate;
  g_getproperty = functions->getproperty;
  g_setproperty = functions->setproperty;
  g_removeproperty = functions->removeproperty;
  g_hasproperty = functions->hasproperty;
  g_hasmethod = functions->hasmethod;
  g_releasevariantvalue = functions->releasevariantvalue;
  g_setexception = functions->setexception;
  g_pushpopupsenabledstate = functions->pushpopupsenabledstate;
  g_poppopupsenabledstate = functions->poppopupsenabledstate;
  g_enumerate = functions->enumerate;
  g_pluginthreadasynccall = functions->pluginthreadasynccall;
  g_construct = functions->construct;
  g_urlredirectresponse = functions->urlredirectresponse;

  if (g_version.v.minor >= NPVERS_HAS_URL_AND_AUTH_INFO) {
    g_getvalueforurl = functions->getvalueforurl;
    g_setvalueforurl = functions->setvalueforurl;
    g_getauthenticationinfo = functions->getauthenticationinfo;
  }
}

void UninitializeBrowserFunctions() {
  g_version.set_version(0);

  // We skip doing this in the official build as it doesn't serve much purpose
// during shutdown.  The reason for it being here in the other types of builds
// is to spot potential browser bugs whereby the browser leaves living objects
// in our DLL after shutdown has been called.  In theory those objects could
// trigger a call to the browser functions after shutdown has been called
// and for non official builds we want that to simply crash.
// For official builds we leave the function pointers around since they
// continue to valid.
  g_geturl = NULL;
  g_posturl = NULL;
  g_requestread = NULL;
  g_newstream = NULL;
  g_write = NULL;
  g_destroystream = NULL;
  g_status = NULL;
  g_useragent = NULL;
  g_memalloc = NULL;
  g_memfree = NULL;
  g_memflush = NULL;
  g_reloadplugins = NULL;
  g_getJavaEnv = NULL;
  g_getJavaPeer = NULL;
  g_geturlnotify = NULL;
  g_posturlnotify = NULL;
  g_getvalue = NULL;
  g_setvalue = NULL;
  g_invalidaterect = NULL;
  g_invalidateregion = NULL;
  g_forceredraw = NULL;
  g_getstringidentifier = NULL;
  g_getstringidentifiers = NULL;
  g_getintidentifier = NULL;
  g_identifierisstring = NULL;
  g_utf8fromidentifier = NULL;
  g_intfromidentifier = NULL;
  g_createobject = NULL;
  g_retainobject = NULL;
  g_releaseobject = NULL;
  g_invoke = NULL;
  g_invoke_default = NULL;
  g_evaluate = NULL;
  g_getproperty = NULL;
  g_setproperty = NULL;
  g_removeproperty = NULL;
  g_hasproperty = NULL;
  g_hasmethod = NULL;
  g_releasevariantvalue = NULL;
  g_setexception = NULL;
  g_pushpopupsenabledstate = NULL;
  g_poppopupsenabledstate = NULL;
  g_enumerate = NULL;
  g_pluginthreadasynccall = NULL;
  g_construct = NULL;
  g_getvalueforurl = NULL;
  g_setvalueforurl = NULL;
  g_getauthenticationinfo = NULL;
}

bool IsInitialized() {
  // We only check one function for convenience.
  return g_getvalue != NULL;
}

// Function stubs for functions that the host browser implements.
uint8 VersionMinor() {
  return g_version.v.minor;
}

uint8 VersionMajor() {
  return g_version.v.major;
}

NPError GetURL(NPP instance, const char* URL, const char* window) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_geturl(instance, URL, window);
}

NPError PostURL(NPP instance, const char* URL, const char* window, uint32 len,
                const char* buf, NPBool file) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_posturl(instance, URL, window, len, buf, file);
}

NPError RequestRead(NPStream* stream, NPByteRange* rangeList) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_requestread(stream, rangeList);
}

NPError NewStream(NPP instance, NPMIMEType type, const char* window,
                  NPStream** stream) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_newstream(instance, type, window, stream);
}

int32 Write(NPP instance, NPStream* stream, int32 len, void* buffer) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_write(instance, stream, len, buffer);
}

NPError DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_destroystream(instance, stream, reason);
}

void Status(NPP instance, const char* message) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_status(instance, message);
}

const char* UserAgent(NPP instance) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_useragent(instance);
}

void* MemAlloc(uint32 size) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_memalloc(size);
}

void MemFree(void* ptr) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_memfree(ptr);
}

uint32 MemFlush(uint32 size) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_memflush(size);
}

void ReloadPlugins(NPBool reloadPages) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_reloadplugins(reloadPages);
}

void* GetJavaEnv() {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_getJavaEnv();
}

void* GetJavaPeer(NPP instance) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_getJavaPeer(instance);
}

NPError GetURLNotify(NPP instance, const char* URL, const char* window,
                     void* notifyData) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_geturlnotify(instance, URL, window, notifyData);
}

NPError PostURLNotify(NPP instance, const char* URL, const char* window,
                      uint32 len, const char* buf, NPBool file,
                      void* notifyData) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_posturlnotify(instance, URL, window, len, buf, file, notifyData);
}

NPError GetValue(NPP instance, NPNVariable variable, void* ret_value) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_getvalue(instance, variable, ret_value);
}

NPError SetValue(NPP instance, NPPVariable variable, void* value) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_setvalue(instance, variable, value);
}

void InvalidateRect(NPP instance, NPRect* rect) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_invalidaterect(instance, rect);
}

void InvalidateRegion(NPP instance, NPRegion region) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_invalidateregion(instance, region);
}

void ForceRedraw(NPP instance) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_forceredraw(instance);
}

void ReleaseVariantValue(NPVariant* variant) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_releasevariantvalue(variant);
}

NPIdentifier GetStringIdentifier(const NPUTF8* name) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_getstringidentifier(name);
}

void GetStringIdentifiers(const NPUTF8** names, int nameCount,
                          NPIdentifier* identifiers) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_getstringidentifiers(names, nameCount, identifiers);
}

NPIdentifier GetIntIdentifier(int32_t intid) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_getintidentifier(intid);
}

int32_t IntFromIdentifier(NPIdentifier identifier) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_intfromidentifier(identifier);
}

bool IdentifierIsString(NPIdentifier identifier) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_identifierisstring(identifier);

}

NPUTF8* UTF8FromIdentifier(NPIdentifier identifier) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_utf8fromidentifier(identifier);

}

NPObject* CreateObject(NPP instance, NPClass* aClass) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_createobject(instance, aClass);

}

NPObject* RetainObject(NPObject* obj) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_retainobject(obj);

}

void ReleaseObject(NPObject* obj) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_releaseobject(obj);

}

bool Invoke(NPP npp, NPObject* obj, NPIdentifier methodName,
            const NPVariant* args, unsigned argCount, NPVariant* result) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_invoke(npp, obj, methodName, args, argCount, result);
}

bool InvokeDefault(NPP npp, NPObject* obj, const NPVariant* args,
                   unsigned argCount, NPVariant* result) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_invoke_default(npp, obj, args, argCount, result);
}

bool Evaluate(NPP npp, NPObject* obj, NPString* script, NPVariant* result) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_evaluate(npp, obj, script, result);
}

bool GetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                 NPVariant* result) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_getproperty(npp, obj, propertyName, result);
}

bool SetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                 const NPVariant* value) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_setproperty(npp, obj, propertyName, value);
}

bool HasProperty(NPP npp, NPObject* npobj, NPIdentifier propertyName) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_hasproperty(npp, npobj, propertyName);
}

bool HasMethod(NPP npp, NPObject* npobj, NPIdentifier methodName) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_hasmethod(npp, npobj, methodName);
}

bool RemoveProperty(NPP npp, NPObject* obj, NPIdentifier propertyName) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_removeproperty(npp, obj, propertyName);
}

void SetException(NPObject* obj, const NPUTF8* message) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_setexception(obj, message);
}

void PushPopupsEnabledState(NPP npp, NPBool enabled) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_pushpopupsenabledstate(npp, enabled);
}

void PopPopupsEnabledState(NPP npp) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_poppopupsenabledstate(npp);
}

bool Enumerate(NPP npp, NPObject* obj, NPIdentifier** identifier,
               uint32_t* count) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_enumerate(npp, obj, identifier, count);
}

void PluginThreadAsyncCall(NPP instance, void (*func)(void*), void* userData) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_pluginthreadasynccall(instance, func, userData);
}

bool Construct(NPP npp, NPObject* obj, const NPVariant* args, uint32_t argCount,
               NPVariant* result) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  return g_construct(npp, obj, args, argCount, result);
}

NPError GetValueForURL(NPP instance, NPNURLVariable variable, const char* url,
                       char** value, uint32* len) {
  DCHECK(IsInitialized()) << __FUNCTION__;
  DCHECK(npapi::VersionMinor() >= NPVERS_HAS_URL_AND_AUTH_INFO);
  if (!g_getvalueforurl) {
    NOTREACHED();
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  return g_getvalueforurl(instance, variable, url, value, len);
}

NPError SetValueForURL(NPP instance, NPNURLVariable variable, const char* url,
                       const char* value, uint32 len)  {
  DCHECK(IsInitialized()) << __FUNCTION__;
  DCHECK(npapi::VersionMinor() >= NPVERS_HAS_URL_AND_AUTH_INFO);
  if (!g_setvalueforurl) {
    NOTREACHED();
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  return g_setvalueforurl(instance, variable, url, value, len);
}

NPError GetAuthenticationInfo(NPP instance, const char* protocol,
                              const char* host, int32 port, const char* scheme,
                              const char *realm, char** username, uint32* ulen,
                              char** password, uint32* plen)  {
  DCHECK(IsInitialized()) << __FUNCTION__;
  DCHECK(npapi::VersionMinor() >= NPVERS_HAS_URL_AND_AUTH_INFO);
  if (g_getauthenticationinfo) {
    NOTREACHED();
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  return g_getauthenticationinfo(instance, protocol, host, port, scheme,
                                 realm, username, ulen, password, plen);
}

void URLRedirectResponse(NPP instance, void* notify_data, NPBool allow) {
  if (!g_urlredirectresponse) {
    NOTREACHED() << "Unexpected call to NPN_URLRedirectResponse";
    return;
  }
  return g_urlredirectresponse(instance, notify_data, allow);
}

std::string StringFromIdentifier(NPIdentifier identifier) {
  std::string ret;
  NPUTF8* utf8 = UTF8FromIdentifier(identifier);
  if (utf8) {
    ret = utf8;
    MemFree(utf8);
  }
  return ret;
}

}  // namespace npapi

void AllocateStringVariant(const std::string& str, NPVariant* var) {
  DCHECK(var);

  int len = str.length();
  NPUTF8* buffer = reinterpret_cast<NPUTF8*>(npapi::MemAlloc(len + 1));
  if (buffer) {
    buffer[len] = '\0';
    memcpy(buffer, str.c_str(), len);
    STRINGN_TO_NPVARIANT(buffer, len, *var);
  } else {
    NULL_TO_NPVARIANT(*var);
  }
}

bool BrowserSupportsRedirectNotification() {
  return npapi::g_urlredirectresponse != NULL;
}
