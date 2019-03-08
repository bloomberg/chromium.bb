// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_loader_factory_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/switches.h"
#include "extensions/common/user_script.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

enum class FactoryUser {
  kContentScript,
  kExtensionProcess,
};

// The allowlist contains HashedExtensionId of extensions discovered via
// Extensions.CrossOriginFetchFromContentScript2 Rappor data.  When the data was
// gathered, these extensions relied on making cross-origin requests from
// content scripts (which requires relaxing CORB).  Going forward, these
// extensions should migrate to making those requests from elsewhere (e.g. from
// a background page, or in the future from extension service workers) at which
// point they can be removed from the allowlist.
//
// Migration plan for extension developers is described at
// https://chromium.org/Home/chromium-security/extension-content-script-fetches
const char* kHardcodedPartOfCorbAllowlist[] = {
    "0149C10F1124F1ED6ACAD85C45E87A76A9DDC667",
    "039F93DD1DF836F1D4E2084C1BEFDB46A854A9D1",
    "072D729E856B1F2C9894AEEC3A5DF65E519D6BEE",
    "07333481B7B8D7F57A9BA64FB98CF86EA87455FC",
    "0EAEA2FDEE025D95B3ABB37014EFF5A98AC4BEAE",
    "109A37B889E7C8AEA7B0103559C3EB6AF73B7832",
    "16A81AEA09A67B03F7AEA5B957D24A4095E764BE",
    "177508B365CBF1610CC2B53707749D79272F2F0B",
    "1AB9CC404876117F49135E67BAD813F935AAE9BA",
    "1DB115A4344B58C0B7CC1595662E86C9E92C0848",
    "260871EABEDE6F2D07D0D9450096093EDAFCBF34",
    "2AA94E2D3F4DA33F0D3BCF5DD48F69B8BDB26F52",
    "3334952C8387B357A41DD8349D39AD9E7C423943",
    "360D8A88545D0C21CBDE2B55EA9E4E4285282B4C",
    "3FDD3DB17F3B686F5A05204700ABA13DF20AE957",
    "43865F8D555C201FE452A5A40702EA96240E749C",
    "4913450195D177430217CAB64B356DC6F6B0F483",
    "505F2C1E723731B2C8C9182FEAA73D00525B2048",
    "61E581B10D83C0AEF8366BB64C18C6615884B3D2",
    "6AE81EF3B13B15080A2DDB23A205A24C65CCC10B",
    "6BA5F75FFF75B69507BC4B09B7094926EF93DBD2",
    "71EE66C0F71CD89BEE340F8568A44101D4C3A9A7",
    "7BFE588B209A15260DE12777B4BBB738DE98FE6C",
    "808FA9BB3CD501D7801D1CD6D5A3DBA088FDD46F",
    "82FDBBF79F3517C3946BD89EAAF90C46DFDA4681",
    "88C372CE52E21560C17BFD52556E60D694E12CAC",
    "88F5F459139892C0F5DF3022676726BB3F01FB5C",
    "8CDD303D6C341D9CAB16713C3CD7587B90A7A84A",
    "934B8F5753A3E5A276FC7D0EE5E575B335A0CC76",
    "973E35633030AD27DABEC99609424A61386C7309",
    "99E06C364BBB2D1F82A9D20BC1645BF21E478259",
    "A30E526CF62131BFBFD7CD9B56253A8F3F171777",
    "A3660FA31A0DBF07C9F80D5342FF215DBC962719",
    "A8FB3967ADE404B77AC3FB5815A399C0660C3C63",
    "A9A4B26C2387BA2A5861C790B0FF39F230427AC8",
    "A9F78610B717B3992F92F490E82FC63FFF46C5FA",
    "AEEDAC793F184240CFB800DA73EE6321E5145102",
    "B4782AE831D849EFCC2AF4BE2012816EDDF8D908",
    "BF5224FB246A6B67EA986EFF77A43F6C1BCA9672",
    "C5BCB9E2E47C3F6FD3F7F83ED982872F77852BA7",
    "CA89BD35059845F2DB4B4398FD339B9F210E9337",
    "CC74B2408753932B5D49C81EC073E3E4CA766EE6",
    "CD8AF9C47DDE6327F8D9A3EFA81F34C6B6C26EBB",
    "CF40F6289951CBFA3B83B792EFA774E2EA06E4C0",
    "D0537B1BADCE856227CE76E31B3772F6B68F653C",
    "E6B12430B6166B31BE20E13941C22569EA75B0F2",
    "E7036E906DBFB77C46EDDEB003A72C0B5CC9BE7F",
    "E873675B8E754F1B1F1222CB921EA14B4335179D",
    "EAD0BFA4ED66D9A2112A4C9A0AA25329FF980AC6",
    "EC24668224116D19FF1A5FFAA61238B88773982C",
    "EC4A841BD03C8E5202043165188A9E060BF703A3",
    "EE4BE5F23D2E59E4713958465941EFB4A18166B7",
    "F566B33D62CE21415AF5B3F3FD8762B7454B8874",
    "F59AB261280AB3AE9826D9359507838B90B07431",
    "F73F9EF0207603992CA3C00A7A0CB223D5571B3F",
    "F9287A33E15038F2591F23E6E9C486717C7202DD",
};

constexpr size_t kHashedExtensionIdLength = base::kSHA1Length * 2;
bool IsValidHashedExtensionId(const std::string& hash) {
  bool correct_chars = std::all_of(hash.begin(), hash.end(), [](char c) {
    return ('A' <= c && c <= 'F') || ('0' <= c && c <= '9');
  });
  bool correct_length = (kHashedExtensionIdLength == hash.length());
  return correct_chars && correct_length;
}

std::vector<std::string> CreateExtensionAllowlist() {
  std::vector<std::string> allowlist;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceEmptyCorbAllowlist)) {
    return allowlist;
  }

  // Make sure kHardcodedPartOfAllowlist will fit, but also leave some room
  // for field trial params.
  allowlist.reserve(base::size(kHardcodedPartOfCorbAllowlist) + 10);

  // Append extensions from the hardcoded allowlist.
  for (const char* hash : kHardcodedPartOfCorbAllowlist) {
    DCHECK(IsValidHashedExtensionId(hash));  // It also validates the length.
    allowlist.push_back(std::string(hash, kHashedExtensionIdLength));
  }

  // Append extensions from the field trial param.
  std::string field_trial_arg = base::GetFieldTrialParamValueByFeature(
      extensions_features::kBypassCorbOnlyForExtensionsAllowlist,
      extensions_features::kBypassCorbAllowlistParamName);
  field_trial_arg = base::ToUpperASCII(field_trial_arg);
  std::vector<std::string> field_trial_allowlist = base::SplitString(
      field_trial_arg, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::EraseIf(field_trial_allowlist, [](const std::string& hash) {
    // Filter out invalid data from |field_trial_allowlist|.
    if (IsValidHashedExtensionId(hash))
      return false;  // Don't remove.

    LOG(ERROR) << "Invalid extension hash: " << hash;
    return true;  // Remove.
  });
  std::move(field_trial_allowlist.begin(), field_trial_allowlist.end(),
            std::back_inserter(allowlist));

  return allowlist;
}

// Returns a set of HashedExtensionId of extensions that depend on relaxed CORB
// behavior in their content scripts.
const base::flat_set<std::string>& GetExtensionsAllowlist() {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kBypassCorbOnlyForExtensionsAllowlist));
  static const base::NoDestructor<base::flat_set<std::string>> s_allowlist([] {
    base::flat_set<std::string> result(CreateExtensionAllowlist());
    result.shrink_to_fit();
    return result;
  }());
  return *s_allowlist;
}

bool DoContentScriptsDependOnRelaxedCorb(const Extension& extension) {
  // Content scripts injected by Chrome Apps (e.g. into <webview> tag) need to
  // run with relaxed CORB.
  if (extension.is_platform_app())
    return true;

  // Content scripts in the current version of extensions might depend on
  // relaxed CORB.
  if (extension.manifest_version() <= 2) {
    if (!base::FeatureList::IsEnabled(
            extensions_features::kBypassCorbOnlyForExtensionsAllowlist))
      return true;

    const std::string& hash = extension.hashed_id().value();
    DCHECK(IsValidHashedExtensionId(hash));
    return base::ContainsKey(GetExtensionsAllowlist(), hash);
  }

  // Safe fallback for future extension manifest versions.
  return false;
}

bool DoExtensionPermissionsCoverCorsOrCorbRelatedOrigins(
    const Extension& extension) {
  // TODO(lukasza): https://crbug.com/846346: Return false if the |extension|
  // doesn't need a special URLLoaderFactory based on |extension| permissions.
  // For now we conservatively assume that all extensions need relaxed CORS/CORB
  // treatment.
  return true;
}

bool IsSpecialURLLoaderFactoryRequired(const Extension& extension,
                                       FactoryUser factory_user) {
  switch (factory_user) {
    case FactoryUser::kContentScript:
      return DoContentScriptsDependOnRelaxedCorb(extension) &&
             DoExtensionPermissionsCoverCorsOrCorbRelatedOrigins(extension);
    case FactoryUser::kExtensionProcess:
      return DoExtensionPermissionsCoverCorsOrCorbRelatedOrigins(extension);
  }
}

network::mojom::URLLoaderFactoryPtrInfo CreateURLLoaderFactory(
    content::RenderProcessHost* process,
    network::mojom::NetworkContext* network_context,
    network::mojom::TrustedURLLoaderHeaderClientPtrInfo* header_client,
    const Extension& extension) {
  // Compute relaxed CORB config to be used by |extension|.
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();

  // Setup factory bound allow list that overwrites per-profile common list
  // to allow tab specific permissions only for this newly created factory.
  params->factory_bound_allow_patterns = CreateCorsOriginAccessAllowList(
      extension,
      PermissionsData::EffectiveHostPermissionsMode::kIncludeTabSpecific);

  if (header_client)
    params->header_client = std::move(*header_client);
  params->process_id = process->GetID();
  // TODO(lukasza): https://crbug.com/846346: Use more granular CORB enforcement
  // based on the specific |extension|'s permissions.
  params->is_corb_enabled = false;
  params->request_initiator_site_lock = url::Origin::Create(extension.url());

  // Create the URLLoaderFactory.
  network::mojom::URLLoaderFactoryPtrInfo factory_info;
  network_context->CreateURLLoaderFactory(mojo::MakeRequest(&factory_info),
                                          std::move(params));
  return factory_info;
}

void MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
    content::RenderFrameHost* frame,
    std::vector<url::Origin> request_initiators,
    bool push_to_renderer_now) {
  DCHECK(!request_initiators.empty());
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    frame->MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
        std::move(request_initiators), push_to_renderer_now);
  } else {
    // TODO(lukasza): In non-NetworkService implementation of CORB, make an
    // exception only for specific extensions (e.g. based on process id,
    // similarly to how r585124 does it for plugins).  Doing so will likely
    // interfere with Extensions.CrossOriginFetchFromContentScript2 Rappor
    // metric, so this needs to wait until this metric is not needed anymore.
  }
}

// If |match_about_blank| is true, then traverses parent/opener chain until the
// first non-about-scheme document and returns its url.  Otherwise, simply
// returns |document_url|.
//
// This function approximates ScriptContext::GetEffectiveDocumentURL from the
// renderer side.  Unlike the renderer version of this code (in
// ScriptContext::GetEffectiveDocumentURL) the code below doesn't consider
// whether security origin of |frame| can access |next_candidate|.  This is
// okay, because our only caller (DoesContentScriptMatchNavigatingFrame) expects
// false positives.
GURL GetEffectiveDocumentURL(content::RenderFrameHost* frame,
                             const GURL& document_url,
                             bool match_about_blank) {
  base::flat_set<content::RenderFrameHost*> already_visited_frames;

  // Common scenario. If |match_about_blank| is false (as is the case in most
  // extensions), or if the frame is not an about:-page, just return
  // |document_url| (supposedly the URL of the frame).
  if (!match_about_blank || !document_url.SchemeIs(url::kAboutScheme))
    return document_url;

  // Non-sandboxed about:blank and about:srcdoc pages inherit their security
  // origin from their parent frame/window. So, traverse the frame/window
  // hierarchy to find the closest non-about:-page and return its URL.
  content::RenderFrameHost* found_frame = frame;
  do {
    DCHECK(found_frame);
    already_visited_frames.insert(found_frame);

    // The loop should only execute (and consider the parent chain) if the
    // currently considered frame has about: scheme.
    DCHECK(match_about_blank);
    DCHECK(
        ((found_frame == frame) && document_url.SchemeIs(url::kAboutScheme)) ||
        (found_frame->GetLastCommittedURL().SchemeIs(url::kAboutScheme)));

    // Attempt to find |next_candidate| - either a parent of opener of
    // |found_frame|.
    content::RenderFrameHost* next_candidate = found_frame->GetParent();
    if (!next_candidate) {
      next_candidate =
          content::WebContents::FromRenderFrameHost(found_frame)->GetOpener();
    }
    if (!next_candidate ||
        base::ContainsKey(already_visited_frames, next_candidate)) {
      break;
    }

    found_frame = next_candidate;
  } while (found_frame->GetLastCommittedURL().SchemeIs(url::kAboutScheme));

  if (found_frame == frame)
    return document_url;  // Not committed yet at ReadyToCommitNavigation time.
  return found_frame->GetLastCommittedURL();
}

// If |user_script| will inject JavaScript content script into the target of
// |navigation|, then DoesContentScriptMatchNavigatingFrame returns true.
// Otherwise it may return either true or false.  Note that this function
// ignores CSS content scripts.
//
// This function approximates a subset of checks from
// UserScriptSet::GetInjectionForScript (which runs in the renderer process).
// Unlike the renderer version, the code below doesn't consider ability to
// create an injection host or the results of ScriptInjector::CanExecuteOnFrame.
// Additionally the |effective_url| calculations are also only an approximation.
// This is okay, because we may return either true even if no content scripts
// would be injected (i.e. it is okay to create a special URLLoaderFactory when
// in reality the content script won't be injected and won't need the factory).
bool DoesContentScriptMatchNavigatingFrame(
    const UserScript& user_script,
    content::RenderFrameHost* navigating_frame,
    const GURL& navigation_target) {
  // A special URLLoaderFactory is only needed for Javascript content scripts
  // (and is never needed for CSS-only injections).
  if (user_script.js_scripts().empty())
    return false;

  GURL effective_url = GetEffectiveDocumentURL(
      navigating_frame, navigation_target, user_script.match_about_blank());
  bool is_subframe = navigating_frame->GetParent();
  return user_script.MatchesDocument(effective_url, is_subframe);
}

}  // namespace

// static
bool URLLoaderFactoryManager::DoContentScriptsMatchNavigatingFrame(
    const Extension& extension,
    content::RenderFrameHost* navigating_frame,
    const GURL& navigation_target) {
  const UserScriptList& list =
      ContentScriptsInfo::GetContentScripts(&extension);
  return std::any_of(list.begin(), list.end(),
                     [navigating_frame, navigation_target](
                         const std::unique_ptr<UserScript>& script) {
                       return DoesContentScriptMatchNavigatingFrame(
                           *script, navigating_frame, navigation_target);
                     });
}

// static
void URLLoaderFactoryManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation) {
  content::RenderFrameHost* frame = navigation->GetRenderFrameHost();
  const GURL& url = navigation->GetURL();

  std::vector<url::Origin> initiators_requiring_separate_factory;
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // ReadyToCommitNavigation shouldn't run during shutdown.
  for (const auto& it : registry->enabled_extensions()) {
    const Extension& extension = *it;
    if (!DoContentScriptsMatchNavigatingFrame(extension, frame, url))
      continue;

    if (!IsSpecialURLLoaderFactoryRequired(extension,
                                           FactoryUser::kContentScript))
      continue;

    initiators_requiring_separate_factory.push_back(
        url::Origin::Create(extension.url()));
  }

  if (!initiators_requiring_separate_factory.empty()) {
    // At ReadyToCommitNavigation time there is no need to trigger an explicit
    // push of URLLoaderFactoryBundle to the renderer - it is sufficient if the
    // factories are pushed slightly later - during the commit.
    constexpr bool kPushToRendererNow = false;

    MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
        frame, std::move(initiators_requiring_separate_factory),
        kPushToRendererNow);
  }
}

// static
void URLLoaderFactoryManager::WillExecuteCode(content::RenderFrameHost* frame,
                                              const HostID& host_id) {
  if (host_id.type() != HostID::EXTENSIONS)
    return;

  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // WillExecuteCode shouldn't happen during shutdown.
  const Extension* extension =
      registry->enabled_extensions().GetByID(host_id.id());
  DCHECK(extension);  // Guaranteed by the caller - see the doc comment.

  if (!IsSpecialURLLoaderFactoryRequired(*extension,
                                         FactoryUser::kContentScript))
    return;

  // When WillExecuteCode runs, the frame already received the initial
  // URLLoaderFactoryBundle - therefore we need to request a separate push
  // below.  This doesn't race with the ExtensionMsg_ExecuteCode message,
  // because the URLLoaderFactoryBundle is sent to the renderer over
  // content.mojom.FrameNavigationControl interface which is associated with the
  // legacy IPC pipe (raciness will be introduced if that ever changes).
  constexpr bool kPushToRendererNow = true;

  MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
      frame, {url::Origin::Create(extension->url())}, kPushToRendererNow);
}

// static
network::mojom::URLLoaderFactoryPtrInfo URLLoaderFactoryManager::CreateFactory(
    content::RenderProcessHost* process,
    network::mojom::NetworkContext* network_context,
    network::mojom::TrustedURLLoaderHeaderClientPtrInfo* header_client,
    const url::Origin& initiator_origin) {
  content::BrowserContext* browser_context = process->GetBrowserContext();
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);  // CreateFactory shouldn't happen during shutdown.

  // Opaque origins normally don't inherit security properties of their
  // precursor origins, but here opaque origins (e.g. think data: URIs) created
  // by an extension should inherit CORS/CORB treatment of the extension.
  url::SchemeHostPort precursor_origin =
      initiator_origin.GetTupleOrPrecursorTupleIfOpaque();

  // Don't create a factory for something that is not an extension.
  if (precursor_origin.scheme() != kExtensionScheme)
    return network::mojom::URLLoaderFactoryPtrInfo();

  // Find the |extension| associated with |initiator_origin|.
  const Extension* extension =
      registry->enabled_extensions().GetByID(precursor_origin.host());
  if (!extension) {
    // This may happen if an extension gets disabled between the time
    // RenderFrameHost::MarkInitiatorAsRequiringSeparateURLLoaderFactory is
    // called and the time
    // ContentBrowserClient::CreateURLLoaderFactory is called.
    return network::mojom::URLLoaderFactoryPtrInfo();
  }

  // Figure out if the factory is needed for content scripts VS extension
  // renderer.
  FactoryUser factory_user = FactoryUser::kContentScript;
  ProcessMap* process_map = ProcessMap::Get(browser_context);
  if (process_map->Contains(extension->id(), process->GetID()))
    factory_user = FactoryUser::kExtensionProcess;

  // Create the factory (but only if really needed).
  if (!IsSpecialURLLoaderFactoryRequired(*extension, factory_user))
    return network::mojom::URLLoaderFactoryPtrInfo();
  return CreateURLLoaderFactory(process, network_context, header_client,
                                *extension);
}

}  // namespace extensions
