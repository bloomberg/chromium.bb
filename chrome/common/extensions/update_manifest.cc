// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/update_manifest.h"

#include <algorithm>

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/libxml_utils.h"
#include "libxml/tree.h"

static const char* kExpectedGupdateProtocol = "2.0";
static const char* kExpectedGupdateXmlns =
    "http://www.google.com/update2/response";

UpdateManifest::UpdateManifest() {}

UpdateManifest::~UpdateManifest() {}

void UpdateManifest::ParseError(const char* details, ...) {
  va_list args;
  va_start(args, details);

  if (errors_.length() > 0) {
    // TODO(asargent) make a platform abstracted newline?
    errors_ += "\r\n";
  }
  StringAppendV(&errors_, details, args);
}

// Checks whether a given node's name matches |expected_name| and
// |expected_namespace|.
static bool TagNameEquals(const xmlNode* node, const char* expected_name,
                          const xmlNs* expected_namespace) {
  if (node->ns != expected_namespace) {
    return false;
  }
  return 0 == strcmp(expected_name, reinterpret_cast<const char*>(node->name));
}

// Returns child nodes of |root| with name |name| in namespace |xml_namespace|.
static std::vector<xmlNode*> GetChildren(xmlNode* root, xmlNs* xml_namespace,
                                         const char* name) {
  std::vector<xmlNode*> result;
  for (xmlNode* child = root->children; child != NULL; child = child->next) {
    if (!TagNameEquals(child, name, xml_namespace)) {
      continue;
    }
    result.push_back(child);
  }
  return result;
}

// Returns the value of a named attribute, or the empty string.
static std::string GetAttribute(xmlNode* node, const char* attribute_name) {
  const xmlChar* name = reinterpret_cast<const xmlChar*>(attribute_name);
  for (xmlAttr* attr = node->properties; attr != NULL; attr = attr->next) {
    if (!xmlStrcmp(attr->name, name) && attr->children &&
        attr->children->content) {
      return std::string(reinterpret_cast<const char*>(
          attr->children->content));
    }
  }
  return std::string();
}

// This is used for the xml parser to report errors. This assumes the context
// is a pointer to a std::string where the error message should be appended.
static void XmlErrorFunc(void *context, const char *message, ...) {
  va_list args;
  va_start(args, message);
  std::string* error = static_cast<std::string*>(context);
  StringAppendV(error, message, args);
}

// Utility class for cleaning up the xml document when leaving a scope.
class ScopedXmlDocument {
 public:
  explicit ScopedXmlDocument(xmlDocPtr document) : document_(document) {}
  ~ScopedXmlDocument() {
    if (document_)
      xmlFreeDoc(document_);
  }

  xmlDocPtr get() {
    return document_;
  }

 private:
  xmlDocPtr document_;
};

// Returns a pointer to the xmlNs on |node| with the |expected_href|, or
// NULL if there isn't one with that href.
static xmlNs* GetNamespace(xmlNode* node, const char* expected_href) {
  const xmlChar* href = reinterpret_cast<const xmlChar*>(expected_href);
  for (xmlNs* ns = node->ns; ns != NULL; ns = ns->next) {
    if (ns->href && !xmlStrcmp(ns->href, href)) {
      return ns;
    }
  }
  return NULL;
}


// Helper function that reads in values for a single <app> tag. It returns a
// boolean indicating success or failure. On failure, it writes a error message
// into |error_detail|.
static bool ParseSingleAppTag(xmlNode* app_node, xmlNs* xml_namespace,
                              UpdateManifest::Result* result,
                              std::string *error_detail) {
  // Read the extension id.
  result->extension_id = GetAttribute(app_node, "appid");
  if (result->extension_id.length() == 0) {
    *error_detail = "Missing appid on app node";
    return false;
  }

  // Get the updatecheck node.
  std::vector<xmlNode*> updates = GetChildren(app_node, xml_namespace,
                                              "updatecheck");
  if (updates.size() > 1) {
    *error_detail = "Too many updatecheck tags on app (expecting only 1).";
    return false;
  }
  if (updates.size() == 0) {
    *error_detail = "Missing updatecheck on app.";
    return false;
  }
  xmlNode *updatecheck = updates[0];

  // Find the url to the crx file.
  result->crx_url = GURL(GetAttribute(updatecheck, "codebase"));
  if (!result->crx_url.is_valid()) {
    *error_detail = "Invalid codebase url: '";
    *error_detail += GetAttribute(updatecheck, "codebase");
    *error_detail += "'.";
    return false;
  }

  // Get the version.
  result->version = GetAttribute(updatecheck, "version");
  if (result->version.length() == 0) {
    *error_detail = "Missing version for updatecheck.";
    return false;
  }
  scoped_ptr<Version> version(Version::GetVersionFromString(result->version));
  if (!version.get()) {
    *error_detail = "Invalid version: '";
    *error_detail += result->version;
    *error_detail += "'.";
    return false;
  }

  // Get the minimum browser version (not required).
  result->browser_min_version = GetAttribute(updatecheck, "prodversionmin");
  if (result->browser_min_version.length()) {
    scoped_ptr<Version> browser_min_version(
      Version::GetVersionFromString(result->browser_min_version));
    if (!browser_min_version.get()) {
      *error_detail = "Invalid prodversionmin: '";
      *error_detail += result->browser_min_version;
      *error_detail += "'.";
      return false;
    }
  }

  // package_hash is optional. It is only required for blacklist. It is a
  // sha256 hash of the package in hex format.
  result->package_hash = GetAttribute(updatecheck, "hash");

  return true;
}


bool UpdateManifest::Parse(const std::string& manifest_xml) {
  results_.resize(0);

  if (manifest_xml.length() < 1) {
    return false;
  }

  std::string xml_errors;
  ScopedXmlErrorFunc error_func(&xml_errors, &XmlErrorFunc);

  // Start up the xml parser with the manifest_xml contents.
  ScopedXmlDocument document(xmlParseDoc(
      reinterpret_cast<const xmlChar*>(manifest_xml.c_str())));
  if (!document.get()) {
    ParseError(xml_errors.c_str());
    return false;
  }

  xmlNode *root = xmlDocGetRootElement(document.get());
  if (!root) {
    ParseError("Missing root node");
    return false;
  }

  // Look for the required namespace declaration.
  xmlNs* gupdate_ns = GetNamespace(root, kExpectedGupdateXmlns);
  if (!gupdate_ns) {
    ParseError("Missing or incorrect xmlns on gupdate tag");
    return false;
  }

  if (!TagNameEquals(root, "gupdate", gupdate_ns)) {
    ParseError("Missing gupdate tag");
    return false;
  }

  // Check for the gupdate "protocol" attribute.
  if (GetAttribute(root, "protocol") != kExpectedGupdateProtocol) {
    ParseError("Missing/incorrect protocol on gupdate tag "
        "(expected '%s')", kExpectedGupdateProtocol);
    return false;
  }

  // Parse each of the <app> tags.
  std::vector<xmlNode*> apps = GetChildren(root, gupdate_ns, "app");
  for (unsigned int i = 0; i < apps.size(); i++) {
    Result current;
    std::string error;
    if (!ParseSingleAppTag(apps[i], gupdate_ns, &current, &error)) {
      ParseError(error.c_str());
      return false;
    }
    results_.push_back(current);
  }

  return true;
}
