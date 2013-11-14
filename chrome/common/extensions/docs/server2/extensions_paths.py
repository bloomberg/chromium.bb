# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from posixpath import join


# Extensions-related paths within the Chromium repository.

EXTENSIONS = 'chrome/common/extensions'

API = join(EXTENSIONS, 'api')
DOCS = join(EXTENSIONS, 'docs')

API_FEATURES = join(API, '_api_features.json')
MANIFEST_FEATURES = join(API, '_manifest_features.json')
PERMISSION_FEATURES = join(API, '_permission_features.json')

EXAMPLES = join(DOCS, 'examples')
SERVER2 = join(DOCS, 'server2')
STATIC_DOCS = join(DOCS, 'static')
TEMPLATES = join(DOCS, 'templates')

APP_YAML = join(SERVER2, 'app.yaml')

ARTICLES_TEMPLATES = join(TEMPLATES, 'articles')
INTROS_TEMPLATES = join(TEMPLATES, 'intros')
JSON_TEMPLATES = join(TEMPLATES, 'json')
PRIVATE_TEMPLATES = join(TEMPLATES, 'private')
PUBLIC_TEMPLATES = join(TEMPLATES, 'public')

CONTENT_PROVIDERS = join(JSON_TEMPLATES, 'content_providers.json')
