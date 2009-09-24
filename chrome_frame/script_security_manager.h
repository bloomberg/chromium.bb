/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef CHROME_FRAME_SCRIPT_SECURITY_MANAGER_H_
#define CHROME_FRAME_SCRIPT_SECURITY_MANAGER_H_

// Gecko headers need this on Windows.
#ifndef XP_WIN
#define XP_WIN
#endif

#include "chrome_frame/ns_associate_iid_win.h"
#include "third_party/xulrunner-sdk/win/include/caps/nsIScriptSecurityManager.h"

ASSOCIATE_IID(NS_ISCRIPTSECURITYMANAGER_IID_STR, nsIScriptSecurityManager);

// Because we need to support both Firefox 3.0.x and 3.5.x, and because Mozilla
// changed these unfrozen interfaces, both are declared here, with specific
// names for specific versions.  Doing this makes it easier to adopt new
// version of the interfaces as they evolve in future version of Firefox.

// The xxx_FF30 declatations below were taken from the file
// nsIScriptSecurityManager.h in the gecko 1.9.0.5 SDK.
// The xxx_FF35 declarations below were taken from the file
// nsIScriptSecurityManager.h in the gecko 1.9.1 SDK.

#define NS_IXPCSECURITYMANAGER_IID_FF30 \
  {0x31431440, 0xf1ce, 0x11d2, \
    { 0x98, 0x5a, 0x00, 0x60, 0x08, 0x96, 0x24, 0x22 }}

class NS_NO_VTABLE nsIXPCSecurityManager_FF30 : public nsISupports {
 public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPCSECURITYMANAGER_IID_FF30)

  /**
    * These flags are used when calling nsIXPConnect::SetSecurityManager
    */
  enum { HOOK_CREATE_WRAPPER = 1U };

  enum { HOOK_CREATE_INSTANCE = 2U };

  enum { HOOK_GET_SERVICE = 4U };

  enum { HOOK_CALL_METHOD = 8U };

  enum { HOOK_GET_PROPERTY = 16U };

  enum { HOOK_SET_PROPERTY = 32U };

  enum { HOOK_ALL = 63U };

  /**
    * For each of these hooks returning NS_OK means 'let the action continue'.
    * Returning an error code means 'veto the action'. XPConnect will return
    * JS_FALSE to the js engine if the action is vetoed. The implementor of this
    * interface is responsible for setting a JS exception into the JSContext
    * if that is appropriate.
    */
  /* void CanCreateWrapper (in JSContextPtr aJSContext, in nsIIDRef aIID,
                            in nsISupports aObj, in nsIClassInfo aClassInfo,
                            inout voidPtr aPolicy); */
  NS_IMETHOD CanCreateWrapper(JSContext*  aJSContext, const nsIID & aIID,
                              nsISupports* aObj, nsIClassInfo* aClassInfo,
                              void** aPolicy) = 0;

  /* void CanCreateInstance (in JSContextPtr aJSContext, in nsCIDRef aCID); */
  NS_IMETHOD CanCreateInstance(JSContext* aJSContext, const nsCID& aCID) = 0;

  /* void CanGetService (in JSContextPtr aJSContext, in nsCIDRef aCID); */
  NS_IMETHOD CanGetService(JSContext* aJSContext, const nsCID& aCID) = 0;

  enum { ACCESS_CALL_METHOD = 0U };

  enum { ACCESS_GET_PROPERTY = 1U };

  enum { ACCESS_SET_PROPERTY = 2U };

  /* void CanAccess (in PRUint32 aAction,
                     in nsAXPCNativeCallContextPtr aCallContext,
                     in JSContextPtr aJSContext, in JSObjectPtr aJSObject,
                     in nsISupports aObj, in nsIClassInfo aClassInfo,
                     in JSVal aName, inout voidPtr aPolicy); */
  NS_IMETHOD CanAccess(PRUint32 aAction,
                       nsAXPCNativeCallContext* aCallContext,
                       JSContext* aJSContext,
                       JSObject* aJSObject,
                       nsISupports* aObj,
                       nsIClassInfo* aClassInfo,
                       jsval aName, void** aPolicy) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPCSecurityManager_FF30,
                              NS_IXPCSECURITYMANAGER_IID_FF30);


#define NS_ISCRIPTSECURITYMANAGER_IID_STR_FF30 \
    "3fffd8e8-3fea-442e-a0ed-2ba81ae197d5"

#define NS_ISCRIPTSECURITYMANAGER_IID_FF30 \
  {0x3fffd8e8, 0x3fea, 0x442e, \
    { 0xa0, 0xed, 0x2b, 0xa8, 0x1a, 0xe1, 0x97, 0xd5 }}

/**
 * WARNING!! The JEP needs to call GetSubjectPrincipal()
 * to support JavaScript-to-Java LiveConnect.  So every change to the
 * nsIScriptSecurityManager interface (big enough to change its IID) also
 * breaks JavaScript-to-Java LiveConnect on mac.
 *
 * If you REALLY have to change this interface, please mark your bug as
 * blocking bug 293973.
 */
class NS_NO_VTABLE NS_SCRIPTABLE nsIScriptSecurityManager_FF30 :
    public nsIXPCSecurityManager_FF30 {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTSECURITYMANAGER_IID_FF30)

  /**
     * Checks whether the running script is allowed to access aProperty.
     */
  /* [noscript] void checkPropertyAccess (in JSContextPtr aJSContext,
                                          in JSObjectPtr aJSObject,
                                          in string aClassName,
                                          in JSVal aProperty,
                                          in PRUint32 aAction); */
  NS_IMETHOD CheckPropertyAccess(JSContext* aJSContext, JSObject* aJSObject,
                                 const char* aClassName, jsval aProperty,
                                 PRUint32 aAction) = 0;

  /**
     * Checks whether the running script is allowed to connect to aTargetURI
     */
  /* [noscript] void checkConnect (in JSContextPtr aJSContext,
                                   in nsIURI aTargetURI, in string aClassName,
                                   in string aProperty); */
  NS_IMETHOD CheckConnect(JSContext* aJSContext, nsIURI* aTargetURI,
                          const char* aClassName, const char* aProperty) = 0;

  /**
     * Check that the script currently running in context "cx" can load "uri".
     *
     * Will return error code NS_ERROR_DOM_BAD_URI if the load request 
     * should be denied.
     *
     * @param cx the JSContext of the script causing the load
     * @param uri the URI that is being loaded
     */
  /* [noscript] void checkLoadURIFromScript (in JSContextPtr cx,
                                             in nsIURI uri); */
  NS_IMETHOD CheckLoadURIFromScript(JSContext* cx, nsIURI* uri) = 0;

  /**
     * Default CheckLoadURI permissions
     */
  enum { STANDARD = 0U };

  enum { LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT = 1U };

  enum { ALLOW_CHROME = 2U };

  enum { DISALLOW_INHERIT_PRINCIPAL = 4U };

  enum { DISALLOW_SCRIPT_OR_DATA = 4U };

  enum { DISALLOW_SCRIPT = 8U };

  /**
     * Check that content with principal aPrincipal can load "uri".
     *
     * Will return error code NS_ERROR_DOM_BAD_URI if the load request 
     * should be denied.
     *
     * @param aPrincipal the principal identifying the actor causing the load
     * @param uri the URI that is being loaded
     * @param flags the permission set, see above
     */
  /* void checkLoadURIWithPrincipal (in nsIPrincipal aPrincipal, in nsIURI uri,
                                     in unsigned long flags); */
  NS_SCRIPTABLE NS_IMETHOD CheckLoadURIWithPrincipal(nsIPrincipal* aPrincipal,
      nsIURI* uri, PRUint32 flags) = 0;

  /**
     * Check that content from "from" can load "uri".
     *
     * Will return error code NS_ERROR_DOM_BAD_URI if the load request 
     * should be denied.
     *
     * @param from the URI causing the load
     * @param uri the URI that is being loaded
     * @param flags the permission set, see above
     *
     * @deprecated Use checkLoadURIWithPrincipal instead of this function.
     */
  /* void checkLoadURI (in nsIURI from, in nsIURI uri,
                        in unsigned long flags); */
  NS_SCRIPTABLE NS_IMETHOD CheckLoadURI(nsIURI* from, nsIURI* uri,
                                        PRUint32 flags) = 0;

  /**
     * Similar to checkLoadURIWithPrincipal but there are two differences:
     *
     * 1) The URI is a string, not a URI object.
     * 2) This function assumes that the URI may still be subject to fixup (and
     * hence will check whether fixed-up versions of the URI are allowed to
     * load as well); if any of the versions of this URI is not allowed, this
     * function will return error code NS_ERROR_DOM_BAD_URI.
     */
  /* void checkLoadURIStrWithPrincipal (in nsIPrincipal aPrincipal,
                                        in AUTF8String uri,
                                        in unsigned long flags); */
  NS_SCRIPTABLE NS_IMETHOD CheckLoadURIStrWithPrincipal(
      nsIPrincipal* aPrincipal, const nsACString& uri, PRUint32 flags) = 0;

  /**
     * Same as CheckLoadURI but takes string arguments for ease of use
     * by scripts
     *
     * @deprecated Use checkLoadURIStrWithPrincipal instead of this function.
     */
  /* void checkLoadURIStr (in AUTF8String from, in AUTF8String uri,
                           in unsigned long flags); */
  NS_SCRIPTABLE NS_IMETHOD CheckLoadURIStr(const nsACString& from,
                                           const nsACString& uri,
                                           PRUint32 flags) = 0;

  /**
     * Check that the function 'funObj' is allowed to run on 'targetObj'
     *
     * Will return error code NS_ERROR_DOM_SECURITY_ERR if the function
     * should not run
     *
     * @param cx The current active JavaScript context.
     * @param funObj The function trying to run..
     * @param targetObj The object the function will run on.
     */
  /* [noscript] void checkFunctionAccess (in JSContextPtr cx,
                                          in voidPtr funObj,
                                          in voidPtr targetObj); */
  NS_IMETHOD CheckFunctionAccess(JSContext* cx, void* funObj,
                                 void* targetObj) = 0;

  /**
     * Return true if content from the given principal is allowed to
     * execute scripts.
     */
  /* [noscript] boolean canExecuteScripts (in JSContextPtr cx,
                                           in nsIPrincipal principal); */
  NS_IMETHOD CanExecuteScripts(JSContext* cx, nsIPrincipal* principal,
                               PRBool* _retval) = 0;

  /**
     * Return the principal of the innermost frame of the currently 
     * executing script. Will return null if there is no script 
     * currently executing.
     */
  /* [noscript] nsIPrincipal getSubjectPrincipal (); */
  NS_IMETHOD GetSubjectPrincipal(nsIPrincipal** _retval) = 0;

  /**
     * Return the all-powerful system principal.
     */
  /* [noscript] nsIPrincipal getSystemPrincipal (); */
  NS_IMETHOD GetSystemPrincipal(nsIPrincipal** _retval) = 0;

  /**
     * Return a principal with the specified certificate fingerprint, subject
     * name (the full name or concatenated set of names of the entity
     * represented by the certificate), pretty name, certificate, and
     * codebase URI.  The certificate fingerprint and subject name MUST be
     * nonempty; otherwise an error will be thrown.  Similarly, aCert must
     * not be null.
     */
  /* [noscript] nsIPrincipal getCertificatePrincipal (
         in AUTF8String aCertFingerprint, in AUTF8String aSubjectName,
         in AUTF8String aPrettyName, in nsISupports aCert, in nsIURI aURI); */
  NS_IMETHOD GetCertificatePrincipal(const nsACString& aCertFingerprint,
                                     const nsACString& aSubjectName,
                                     const nsACString& aPrettyName,
                                     nsISupports* aCert,
                                     nsIURI* aURI, nsIPrincipal** _retval) = 0;

  /**
     * Return a principal that has the same origin as aURI.
     */
  /* nsIPrincipal getCodebasePrincipal (in nsIURI aURI); */
  NS_SCRIPTABLE NS_IMETHOD GetCodebasePrincipal(nsIURI* aURI,
                                                nsIPrincipal **_retval) = 0;

  /**
     * Request that 'capability' can be enabled by scripts or applets
     * running with 'principal'. Will prompt user if
     * necessary. Returns nsIPrincipal::ENABLE_GRANTED or
     * nsIPrincipal::ENABLE_DENIED based on user's choice.
     */
  /* [noscript] short requestCapability (in nsIPrincipal principal,
                                         in string capability); */
  NS_IMETHOD RequestCapability(nsIPrincipal* principal,
                               const char* capability, PRInt16* _retval) = 0;

  /**
     * Return true if the currently executing script has 'capability' enabled.
     */
  /* boolean isCapabilityEnabled (in string capability); */
  NS_SCRIPTABLE NS_IMETHOD IsCapabilityEnabled(const char* capability,
                                               PRBool* _retval) = 0;

  /**
     * Enable 'capability' in the innermost frame of the currently executing
     * script.
     */
  /* void enableCapability (in string capability); */
  NS_SCRIPTABLE NS_IMETHOD EnableCapability(const char* capability) = 0;

  /**
     * Remove 'capability' from the innermost frame of the currently
     * executing script. Any setting of 'capability' from enclosing
     * frames thus comes into effect.
     */
  /* void revertCapability (in string capability); */
  NS_SCRIPTABLE NS_IMETHOD RevertCapability(const char* capability) = 0;

  /**
     * Disable 'capability' in the innermost frame of the currently executing
     * script.
     */
  /* void disableCapability (in string capability); */
  NS_SCRIPTABLE NS_IMETHOD DisableCapability(const char* capability) = 0;

  /**
     * Allow 'certificateID' to enable 'capability.' Can only be performed
     * by code signed by the system certificate.
     */
  /* void setCanEnableCapability (in AUTF8String certificateFingerprint,
                                  in string capability, in short canEnable); */
  NS_SCRIPTABLE NS_IMETHOD SetCanEnableCapability(
      const nsACString& certificateFingerprint, const char* capability,
      PRInt16 canEnable) = 0;

  /**
     * Return the principal of the specified object in the specified context.
     */
  /* [noscript] nsIPrincipal getObjectPrincipal (in JSContextPtr cx,
                                                 in JSObjectPtr obj); */
  NS_IMETHOD GetObjectPrincipal(JSContext* cx, JSObject* obj,
                                nsIPrincipal** _retval) = 0;

  /**
     * Returns true if the principal of the currently running script is the
     * system principal, false otherwise.
     */
  /* [noscript] boolean subjectPrincipalIsSystem (); */
  NS_IMETHOD SubjectPrincipalIsSystem(PRBool* _retval) = 0;

  /**
     * Returns OK if aJSContext and target have the same "origin"
     * (scheme, host, and port).
     */
  /* [noscript] void checkSameOrigin (in JSContextPtr aJSContext,
                                      in nsIURI aTargetURI); */
  NS_IMETHOD CheckSameOrigin(JSContext* aJSContext, nsIURI* aTargetURI) = 0;

  /**
     * Returns OK if aSourceURI and target have the same "origin"
     * (scheme, host, and port).
     * ReportError flag suppresses error reports for functions that
     * don't need reporting.
     */
  /* void checkSameOriginURI (in nsIURI aSourceURI, in nsIURI aTargetURI,
                              in boolean reportError); */
  NS_SCRIPTABLE NS_IMETHOD CheckSameOriginURI(nsIURI* aSourceURI,
                                              nsIURI* aTargetURI,
                                              PRBool reportError) = 0;

  /**
     * Returns the principal of the global object of the given context, or null
     * if no global or no principal.
     */
  /* [noscript] nsIPrincipal getPrincipalFromContext (in JSContextPtr cx); */
  NS_IMETHOD GetPrincipalFromContext(JSContext* cx,
                                     nsIPrincipal** _retval) = 0;

  /**
     * Get the principal for the given channel.  This will typically be the
     * channel owner if there is one, and the codebase principal for the
     * channel's URI otherwise.  aChannel must not be null.
     */
  /* nsIPrincipal getChannelPrincipal (in nsIChannel aChannel); */
  NS_SCRIPTABLE NS_IMETHOD GetChannelPrincipal(nsIChannel* aChannel,
                                               nsIPrincipal** _retval) = 0;

  /**
     * Check whether a given principal is a system principal.  This allows us
     * to avoid handing back the system principal to script while allowing
     * script to check whether a given principal is system.
     */
  /* boolean isSystemPrincipal (in nsIPrincipal aPrincipal); */
  NS_SCRIPTABLE NS_IMETHOD IsSystemPrincipal(nsIPrincipal* aPrincipal,
                                             PRBool* _retval) = 0;

  /**
     * Same as getSubjectPrincipal(), only faster. cx must *never* be
     * passed null, and it must be the context on the top of the
     * context stack. Does *not* reference count the returned
     * principal.
     */
  /* [noscript, notxpcom] nsIPrincipal getCxSubjectPrincipal (
         in JSContextPtr cx); */
  NS_IMETHOD_(nsIPrincipal *) GetCxSubjectPrincipal(JSContext* cx) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptSecurityManager_FF30,
                              NS_ISCRIPTSECURITYMANAGER_IID_FF30);

ASSOCIATE_IID(NS_ISCRIPTSECURITYMANAGER_IID_STR_FF30,
              nsIScriptSecurityManager_FF30);


#define NS_ISCRIPTSECURITYMANAGER_IID_STR_FF35 \
    "f8e350b9-9f31-451a-8c8f-d10fea26b780"

#define NS_ISCRIPTSECURITYMANAGER_IID_FF35 \
  {0xf8e350b9, 0x9f31, 0x451a, \
    { 0x8c, 0x8f, 0xd1, 0x0f, 0xea, 0x26, 0xb7, 0x80 }}

#ifndef NS_OUTPARAM
#define NS_OUTPARAM
#endif

/**
 * WARNING!! The JEP needs to call GetSubjectPrincipal()
 * to support JavaScript-to-Java LiveConnect.  So every change to the
 * nsIScriptSecurityManager interface (big enough to change its IID) also
 * breaks JavaScript-to-Java LiveConnect on mac.
 *
 * If you REALLY have to change this interface, please mark your bug as
 * blocking bug 293973.
 */
class NS_NO_VTABLE NS_SCRIPTABLE nsIScriptSecurityManager_FF35 :
    public nsIXPCSecurityManager_FF30 {
 public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTSECURITYMANAGER_IID_FF35)

  /**
     * Checks whether the running script is allowed to access aProperty.
     */
  /* [noscript] void checkPropertyAccess (in JSContextPtr aJSContext,
                                          in JSObjectPtr aJSObject,
                                          in string aClassName,
                                          in JSVal aProperty,
                                          in PRUint32 aAction); */
  NS_IMETHOD CheckPropertyAccess(JSContext* aJSContext, JSObject* aJSObject,
                                 const char* aClassName, jsval aProperty,
                                 PRUint32 aAction) = 0;

  /**
     * Checks whether the running script is allowed to connect to aTargetURI
     */
  /* [noscript] void checkConnect (in JSContextPtr aJSContext,
                                   in nsIURI aTargetURI, in string aClassName,
                                   in string aProperty); */
  NS_IMETHOD CheckConnect(JSContext* aJSContext, nsIURI* aTargetURI,
                          const char* aClassName, const char* aProperty) = 0;

  /**
     * Check that the script currently running in context "cx" can load "uri".
     *
     * Will return error code NS_ERROR_DOM_BAD_URI if the load request
     * should be denied.
     *
     * @param cx the JSContext of the script causing the load
     * @param uri the URI that is being loaded
     */
  /* [noscript] void checkLoadURIFromScript (in JSContextPtr cx,
                                             in nsIURI uri); */
  NS_IMETHOD CheckLoadURIFromScript(JSContext* cx, nsIURI* uri) = 0;

  /**
     * Default CheckLoadURI permissions
     */
  enum { STANDARD = 0U };

  enum { LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT = 1U };

  enum { ALLOW_CHROME = 2U };

  enum { DISALLOW_INHERIT_PRINCIPAL = 4U };

  enum { DISALLOW_SCRIPT_OR_DATA = 4U };

  enum { DISALLOW_SCRIPT = 8U };

  /**
     * Check that content with principal aPrincipal can load "uri".
     *
     * Will return error code NS_ERROR_DOM_BAD_URI if the load request
     * should be denied.
     *
     * @param aPrincipal the principal identifying the actor causing the load
     * @param uri the URI that is being loaded
     * @param flags the permission set, see above
     */
  /* void checkLoadURIWithPrincipal (in nsIPrincipal aPrincipal, in nsIURI uri,
                                     in unsigned long flags); */
  NS_SCRIPTABLE NS_IMETHOD CheckLoadURIWithPrincipal(nsIPrincipal* aPrincipal,
                                                     nsIURI* uri,
                                                     PRUint32 flags) = 0;

  /**
     * Check that content from "from" can load "uri".
     *
     * Will return error code NS_ERROR_DOM_BAD_URI if the load request
     * should be denied.
     *
     * @param from the URI causing the load
     * @param uri the URI that is being loaded
     * @param flags the permission set, see above
     *
     * @deprecated Use checkLoadURIWithPrincipal instead of this function.
     */
  /* void checkLoadURI (in nsIURI from, in nsIURI uri,
                        in unsigned long flags); */
  NS_SCRIPTABLE NS_IMETHOD CheckLoadURI(nsIURI* from, nsIURI* uri,
                                        PRUint32 flags) = 0;

  /**
     * Similar to checkLoadURIWithPrincipal but there are two differences:
     *
     * 1) The URI is a string, not a URI object.
     * 2) This function assumes that the URI may still be subject to fixup (and
     * hence will check whether fixed-up versions of the URI are allowed to
     * load as well); if any of the versions of this URI is not allowed, this
     * function will return error code NS_ERROR_DOM_BAD_URI.
     */
  /* void checkLoadURIStrWithPrincipal (in nsIPrincipal aPrincipal,
                                        in AUTF8String uri,
                                        in unsigned long flags); */
  NS_SCRIPTABLE NS_IMETHOD CheckLoadURIStrWithPrincipal(
      nsIPrincipal* aPrincipal, const nsACString& uri, PRUint32 flags) = 0;

  /**
     * Same as CheckLoadURI but takes string arguments for ease of use
     * by scripts
     *
     * @deprecated Use checkLoadURIStrWithPrincipal instead of this function.
     */
  /* void checkLoadURIStr (in AUTF8String from, in AUTF8String uri,
                           in unsigned long flags); */
  NS_SCRIPTABLE NS_IMETHOD CheckLoadURIStr(const nsACString& from,
                                           const nsACString & uri,
                                           PRUint32 flags) = 0;

  /**
     * Check that the function 'funObj' is allowed to run on 'targetObj'
     *
     * Will return error code NS_ERROR_DOM_SECURITY_ERR if the function
     * should not run
     *
     * @param cx The current active JavaScript context.
     * @param funObj The function trying to run..
     * @param targetObj The object the function will run on.
     */
  /* [noscript] void checkFunctionAccess (in JSContextPtr cx,
                                          in voidPtr funObj,
                                          in voidPtr targetObj); */
  NS_IMETHOD CheckFunctionAccess(JSContext* cx, void* funObj,
                                 void* targetObj) = 0;

  /**
     * Return true if content from the given principal is allowed to
     * execute scripts.
     */
  /* [noscript] boolean canExecuteScripts (in JSContextPtr cx,
                                           in nsIPrincipal principal); */
  NS_IMETHOD CanExecuteScripts(JSContext* cx, nsIPrincipal* principal,
                               PRBool* _retval NS_OUTPARAM) = 0;

  /**
     * Return the principal of the innermost frame of the currently
     * executing script. Will return null if there is no script
     * currently executing.
     */
  /* [noscript] nsIPrincipal getSubjectPrincipal (); */
  NS_IMETHOD GetSubjectPrincipal(nsIPrincipal **_retval NS_OUTPARAM) = 0;

  /**
     * Return the all-powerful system principal.
     */
  /* [noscript] nsIPrincipal getSystemPrincipal (); */
  NS_IMETHOD GetSystemPrincipal(nsIPrincipal **_retval NS_OUTPARAM) = 0;

  /**
     * Return a principal with the specified certificate fingerprint, subject
     * name (the full name or concatenated set of names of the entity
     * represented by the certificate), pretty name, certificate, and
     * codebase URI.  The certificate fingerprint and subject name MUST be
     * nonempty; otherwise an error will be thrown.  Similarly, aCert must
     * not be null.
     */
  /* [noscript] nsIPrincipal getCertificatePrincipal (
         in AUTF8String aCertFingerprint, in AUTF8String aSubjectName,
         in AUTF8String aPrettyName, in nsISupports aCert, in nsIURI aURI); */
  NS_IMETHOD GetCertificatePrincipal(const nsACString& aCertFingerprint,
                                     const nsACString& aSubjectName,
                                     const nsACString& aPrettyName,
                                     nsISupports* aCert, nsIURI *aURI,
                                     nsIPrincipal** _retval NS_OUTPARAM) = 0;

  /**
     * Return a principal that has the same origin as aURI.
     */
  /* nsIPrincipal getCodebasePrincipal (in nsIURI aURI); */
  NS_SCRIPTABLE NS_IMETHOD GetCodebasePrincipal(nsIURI* aURI,
      nsIPrincipal** _retval NS_OUTPARAM) = 0;

  /**
     * Request that 'capability' can be enabled by scripts or applets
     * running with 'principal'. Will prompt user if
     * necessary. Returns nsIPrincipal::ENABLE_GRANTED or
     * nsIPrincipal::ENABLE_DENIED based on user's choice.
     */
  /* [noscript] short requestCapability (in nsIPrincipal principal,
                                         in string capability); */
  NS_IMETHOD RequestCapability(nsIPrincipal* principal, const char* capability,
                               PRInt16* _retval NS_OUTPARAM) = 0;

  /**
     * Return true if the currently executing script has 'capability' enabled.
     */
  /* boolean isCapabilityEnabled (in string capability); */
  NS_SCRIPTABLE NS_IMETHOD IsCapabilityEnabled(const char* capability,
                                               PRBool* _retval NS_OUTPARAM) = 0;

  /**
     * Enable 'capability' in the innermost frame of the currently executing
     * script.
     */
  /* void enableCapability (in string capability); */
  NS_SCRIPTABLE NS_IMETHOD EnableCapability(const char* capability) = 0;

  /**
     * Remove 'capability' from the innermost frame of the currently
     * executing script. Any setting of 'capability' from enclosing
     * frames thus comes into effect.
     */
  /* void revertCapability (in string capability); */
  NS_SCRIPTABLE NS_IMETHOD RevertCapability(const char* capability) = 0;

  /**
     * Disable 'capability' in the innermost frame of the currently executing
     * script.
     */
  /* void disableCapability (in string capability); */
  NS_SCRIPTABLE NS_IMETHOD DisableCapability(const char* capability) = 0;

  /**
     * Allow 'certificateID' to enable 'capability.' Can only be performed
     * by code signed by the system certificate.
     */
  /* void setCanEnableCapability (in AUTF8String certificateFingerprint,
                                  in string capability, in short canEnable); */
  NS_SCRIPTABLE NS_IMETHOD SetCanEnableCapability(
      const nsACString& certificateFingerprint, const char *capability,
      PRInt16 canEnable) = 0;

  /**
     * Return the principal of the specified object in the specified context.
     */
  /* [noscript] nsIPrincipal getObjectPrincipal (in JSContextPtr cx,
                                                 in JSObjectPtr obj); */
  NS_IMETHOD GetObjectPrincipal(JSContext* cx, JSObject* obj,
                                nsIPrincipal** _retval NS_OUTPARAM) = 0;

  /**
     * Returns true if the principal of the currently running script is the
     * system principal, false otherwise.
     */
  /* [noscript] boolean subjectPrincipalIsSystem (); */
  NS_IMETHOD SubjectPrincipalIsSystem(PRBool* _retval NS_OUTPARAM) = 0;

  /**
     * Returns OK if aJSContext and target have the same "origin"
     * (scheme, host, and port).
     */
  /* [noscript] void checkSameOrigin (in JSContextPtr aJSContext,
                                      in nsIURI aTargetURI); */
  NS_IMETHOD CheckSameOrigin(JSContext* aJSContext, nsIURI* aTargetURI) = 0;

  /**
     * Returns OK if aSourceURI and target have the same "origin"
     * (scheme, host, and port).
     * ReportError flag suppresses error reports for functions that
     * don't need reporting.
     */
  /* void checkSameOriginURI (in nsIURI aSourceURI, in nsIURI aTargetURI,
                              in boolean reportError); */
  NS_SCRIPTABLE NS_IMETHOD CheckSameOriginURI(nsIURI* aSourceURI,
                                              nsIURI* aTargetURI,
                                              PRBool reportError) = 0;

  /**
     * Returns the principal of the global object of the given context, or null
     * if no global or no principal.
     */
  /* [noscript] nsIPrincipal getPrincipalFromContext (in JSContextPtr cx); */
  NS_IMETHOD GetPrincipalFromContext(JSContext* cx,
                                     nsIPrincipal** _retval NS_OUTPARAM) = 0;

  /**
     * Get the principal for the given channel.  This will typically be the
     * channel owner if there is one, and the codebase principal for the
     * channel's URI otherwise.  aChannel must not be null.
     */
  /* nsIPrincipal getChannelPrincipal (in nsIChannel aChannel); */
  NS_SCRIPTABLE NS_IMETHOD GetChannelPrincipal(nsIChannel* aChannel,
      nsIPrincipal** _retval NS_OUTPARAM) = 0;

  /**
     * Check whether a given principal is a system principal.  This allows us
     * to avoid handing back the system principal to script while allowing
     * script to check whether a given principal is system.
     */
  /* boolean isSystemPrincipal (in nsIPrincipal aPrincipal); */
  NS_SCRIPTABLE NS_IMETHOD IsSystemPrincipal(nsIPrincipal* aPrincipal,
                                             PRBool* _retval NS_OUTPARAM) = 0;

  /**
     * Same as getSubjectPrincipal(), only faster. cx must *never* be
     * passed null, and it must be the context on the top of the
     * context stack. Does *not* reference count the returned
     * principal.
     */
  /* [noscript, notxpcom] nsIPrincipal getCxSubjectPrincipal (
         in JSContextPtr cx); */
  NS_IMETHOD_(nsIPrincipal*) GetCxSubjectPrincipal(JSContext* cx) = 0;

  /* [noscript, notxpcom] nsIPrincipal getCxSubjectPrincipalAndFrame (in JSContextPtr cx, out JSStackFramePtr fp); */
  NS_IMETHOD_(nsIPrincipal*) GetCxSubjectPrincipalAndFrame(JSContext* cx,
      JSStackFrame** fp NS_OUTPARAM) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptSecurityManager_FF35,
                              NS_ISCRIPTSECURITYMANAGER_IID_FF35);

ASSOCIATE_IID(NS_ISCRIPTSECURITYMANAGER_IID_STR_FF35,
              nsIScriptSecurityManager_FF35);

#endif  // CHROME_FRAME_SCRIPT_SECURITY_MANAGER_H_
