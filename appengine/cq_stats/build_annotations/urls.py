# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Url disptacher for the build_annotations app."""

from __future__ import print_function

from django.conf import urls

from build_annotations import views


urlpatterns = urls.patterns(
    '',
    urls.url(r'^$', views.ListBuildsView.as_view(),
             name='builds_list'),
    urls.url(r'edit_annotations/(?P<build_id>\d+)/$',
             views.EditAnnotationsView.as_view(),
             name='edit_annotations'))
