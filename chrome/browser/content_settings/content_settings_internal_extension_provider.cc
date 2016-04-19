// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_internal_extension_provider.h"

#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/simple_feature.h"

using extensions::UnloadedExtensionInfo;

namespace content_settings {

namespace {

// This is the set of extensions that are allowed to access the internal
// remoting viewer plugin.
const char* const kRemotingViewerWhitelist[] = {
    "gbchcmhmhahfdphkhkmpfmihenigjmpp",  // Chrome Remote Desktop
    "kgngmbheleoaphbjbaiobfdepmghbfah",  // Pre-release Chrome Remote Desktop
    "odkaodonbgfohohmklejpjiejmcipmib",  // Dogfood Chrome Remote Desktop
    "ojoimpklfciegopdfgeenehpalipignm",  // Chromoting canary
};

}  // namespace

InternalExtensionProvider::InternalExtensionProvider(Profile* profile)
    : registrar_(new content::NotificationRegistrar),
      extension_registry_(extensions::ExtensionRegistry::Get(profile)) {
  for (size_t i = 0; i < arraysize(kRemotingViewerWhitelist); ++i)
    chrome_remote_desktop_.insert(kRemotingViewerWhitelist[i]);

  // Whitelist all extensions loaded so far.
  for (const scoped_refptr<const extensions::Extension>& extension :
       extensions::ExtensionRegistry::Get(profile)->enabled_extensions()) {
    ApplyPluginContentSettingsForExtension(extension.get(),
                                           CONTENT_SETTING_ALLOW);
  }
  registrar_->Add(this,
                  extensions::NOTIFICATION_EXTENSION_HOST_CREATED,
                  content::Source<Profile>(profile));
  extension_registry_->AddObserver(this);
}

InternalExtensionProvider::~InternalExtensionProvider() {
  DCHECK(!registrar_.get());
}

std::unique_ptr<RuleIterator> InternalExtensionProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return value_map_.GetRuleIterator(content_type, resource_identifier, &lock_);
}

bool InternalExtensionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Value* value) {
  return false;
}

void InternalExtensionProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {}

void InternalExtensionProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_HOST_CREATED, type);
  const extensions::ExtensionHost* host =
      content::Details<extensions::ExtensionHost>(details).ptr();
  if (host->extension()->is_platform_app()) {
    SetContentSettingForExtension(host->extension(), CONTENT_SETTING_BLOCK);

    // White-list CRD's v2 app, until crbug.com/134216 is complete.
    const char* const kAppWhitelist[] = {
        "2775E568AC98F9578791F1EAB65A1BF5F8CEF414",
        "4AA3C5D69A4AECBD236CAD7884502209F0F5C169",
        "97B23E01B2AA064E8332EE43A7A85C628AADC3F2",
        "9E930B2B5EABA6243AE6C710F126E54688E8FAF6",
        "C449A798C495E6CF7D6AF10162113D564E67AD12",
        "E410CDAB2C6E6DD408D731016CECF2444000A912",
        "EBA908206905323CECE6DC4B276A58A0F4AC573F",

        // http://crbug.com/327507
        "FAFE8EFDD2D6AE2EEB277AFEB91C870C79064D9E",
        "3B52D273A271D4E2348233E322426DBAE854B567",
        "5DF6ADC8708DF59FCFDDBF16AFBFB451380C2059",
        "1037DEF5F6B06EA46153AD87B6C5C37440E3F2D1",
        "F5815DAFEB8C53B078DD1853B2059E087C42F139",
        "6A08EFFF9C16E090D6DCC7EC55A01CADAE840513",

        // http://crbug.com/354258
        "C32D6D93E12F5401DAA3A723E0C3CC5F25429BA4",
        "9099782647D39C778E15C8C6E0D23C88F5CDE170",
        "B7D5B52D1E5B106288BD7F278CAFA5E8D76108B0",
        "89349DBAA2C4022FB244AA50182AB60934EB41EE",
        "CB593E510640572A995CB1B6D41BD85ED51E63F8",
        "1AD1AC86C87969CD3434FA08D99DBA6840AEA612",
        "9C2EA21D7975BDF2B3C01C3A454EE44854067A6D",
        "D2C488C80C3C90C3E01A991112A05E37831E17D0",
        "6EEC061C0E74B46C7B5BE2EEFA49436368F4988F",
        "8B344D9E8A4C505EF82A0DBBC25B8BD1F984E777",
        "E06AFCB1EB0EFD237824CC4AC8FDD3D43E8BC868",

        // http://crbug.com/386324
        "F76F43EFFF56BF17A9868A5243F339BA28746632",
        "C6EA52B92F80878515F94137020F01519357E5B5",
        "E466389F058ABD73FF6FDD06F768A351FCBF8532",
        "40063F1CF7B68BA847A26FA6620DDF156171D23C",
        "A6FD8E15353CF1F5C3D0A7B20E1D10AEA4DD3E6A",
        "57AC4D9E6BD8A2D0A70056B5FAC2378CAA588912",
        "02037314DA4D913640DCF0E296A7D01F4FD793EC",
        "B6EC0809BC63E10B431C5E4AA3645232CA86B2A5",
        "48CA541313139786F056DBCB504A1025CFF5D2E3",
        "05106136AE7F08A3C181D4648E5438350B1D2B4F"};
    if (extensions::SimpleFeature::IsIdInArray(
            host->extension()->id(), kAppWhitelist, arraysize(kAppWhitelist))) {
      SetContentSettingForExtensionAndResource(
          host->extension(), ChromeContentClient::kRemotingViewerPluginPath,
          CONTENT_SETTING_ALLOW);
    }
  }
}

void InternalExtensionProvider::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  ApplyPluginContentSettingsForExtension(extension, CONTENT_SETTING_ALLOW);
}

void InternalExtensionProvider::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  ApplyPluginContentSettingsForExtension(extension, CONTENT_SETTING_DEFAULT);
}

void InternalExtensionProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
  registrar_.reset();
  extension_registry_->RemoveObserver(this);
}

void InternalExtensionProvider::ApplyPluginContentSettingsForExtension(
    const extensions::Extension* extension,
    ContentSetting setting) {
  if (extensions::PluginInfo::HasPlugins(extension))
    SetContentSettingForExtension(extension, setting);

  // Chrome Remove Desktop relies on the remoting viewer plugin. The
  // above check does not catch this, as the plugin is a builtin plugin.
  if (chrome_remote_desktop_.find(extension->id()) !=
      chrome_remote_desktop_.end()) {
    SetContentSettingForExtensionAndResource(
        extension, ChromeContentClient::kRemotingViewerPluginPath, setting);
  }

  // The PDF viewer extension relies on the out of process PDF plugin.
  if (extension->id() == extension_misc::kPdfExtensionId) {
    SetContentSettingForExtensionAndResource(
        extension, pdf_extension_util::kPdfResourceIdentifier, setting);
  }
}

void InternalExtensionProvider::SetContentSettingForExtension(
    const extensions::Extension* extension,
    ContentSetting setting) {
  SetContentSettingForExtensionAndResource(
      extension, ResourceIdentifier(), setting);
}

void InternalExtensionProvider::SetContentSettingForExtensionAndResource(
    const extensions::Extension* extension,
    const ResourceIdentifier& resource,
    ContentSetting setting) {
  std::unique_ptr<ContentSettingsPattern::BuilderInterface> pattern_builder(
      ContentSettingsPattern::CreateBuilder(false));
  pattern_builder->WithScheme(extensions::kExtensionScheme);
  pattern_builder->WithHost(extension->id());
  pattern_builder->WithPathWildcard();

  ContentSettingsPattern primary_pattern = pattern_builder->Build();
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();
  {
    base::AutoLock lock(lock_);
    if (setting == CONTENT_SETTING_DEFAULT) {
      value_map_.DeleteValue(primary_pattern,
                             secondary_pattern,
                             CONTENT_SETTINGS_TYPE_PLUGINS,
                             resource);
    } else {
      value_map_.SetValue(primary_pattern,
                          secondary_pattern,
                          CONTENT_SETTINGS_TYPE_PLUGINS,
                          resource,
                          new base::FundamentalValue(setting));
    }
  }
  NotifyObservers(primary_pattern,
                  secondary_pattern,
                  CONTENT_SETTINGS_TYPE_PLUGINS,
                  resource);
}

}  // namespace content_settings
