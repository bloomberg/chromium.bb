# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""All django views for the build_annotations app."""

from __future__ import print_function

from django import http
from django import shortcuts
from django.core import urlresolvers
from django.views import generic

from build_annotations import build_row_controller
from build_annotations import models as ba_models
from build_annotations import forms as ba_forms


class ListBuildsView(generic.list.ListView):
  """The landing page view of the app. Lists requested builds."""

  template_name = 'build_annotations/index.html'

  def __init__(self, *args, **kwargs):
    super(ListBuildsView, self).__init__(*args, **kwargs)
    self._search_form = None
    self._controller = None
    self._session = None

  def get_queryset(self):
    self._EnsureSessionInitialized()
    self._controller = build_row_controller.BuildRowController()
    return self._controller.GetStructuredBuilds(
        latest_build_id=self._session['latest_build_id'],
        num_builds=self._session['num_builds'])

  def get_context_object_name(self, _):
    return 'builds_list'

  def get_context_data(self, **kwargs):
    context = super(ListBuildsView, self).get_context_data(**kwargs)
    context['search_form'] = self._GetSearchForm()
    context['latest_build_id_cached'] = self._GetLatestBuildId()
    return context

  def get(self, request):
    self._session = request.session
    return super(ListBuildsView, self).get(request)

  def post(self, request):
    self._session = request.session
    form = ba_forms.SearchForm(request.POST)
    self._search_form = form
    if form.is_valid():
      self._session['latest_build_id'] = form.cleaned_data['latest_build_id']
      self._session['num_builds'] = form.cleaned_data['num_builds']
    return self.get(request)

  def put(self, *args, **kwargs):
    return self.post(*args, **kwargs)

  def _GetSearchForm(self):
    if self._search_form is not None:
      return self._search_form
    return ba_forms.SearchForm(
        {'latest_build_id': self._session['latest_build_id'],
         'num_builds': self._session['num_builds']})

  def _EnsureSessionInitialized(self):
    latest_build_id = self._session.get('latest_build_id', None)
    num_results = self._session.get('num_builds', None)
    if latest_build_id is None or num_results is None:
      # We don't have a valid search history in this session, obtain defaults.
      controller = build_row_controller.BuildRowController()
      controller.GetStructuredBuilds(num_builds=1)
      self._session['latest_build_id'] = controller.latest_build_id
      self._session['num_builds'] = controller.DEFAULT_NUM_BUILDS

  def _GetLatestBuildId(self):
    controller = build_row_controller.BuildRowController()
    controller.GetStructuredBuilds(num_builds=1)
    return controller.latest_build_id


class EditAnnotationsView(generic.base.View):
  """View that handles annotation editing page."""

  template_name = 'build_annotations/edit_annotations.html'

  def __init__(self, *args, **kwargs):
    self._formset = None
    self._annotator = 'default_annotator'
    self._context = {}
    self._request = None
    self._session = None
    self._build_id = None
    super(EditAnnotationsView, self).__init__(*args, **kwargs)

  def get(self, request, build_id):
    self._request = request
    self._build_id = build_id
    self._session = request.session
    self._PopulateContext()
    return shortcuts.render(request, self.template_name, self._context)

  def post(self, request, build_id):
    self._request = request
    self._build_id = build_id
    self._session = request.session
    self._formset = ba_forms.AnnotationsFormSet(request.POST)
    if self._formset.is_valid():
      self._SaveAnnotations()
      return http.HttpResponseRedirect(
          urlresolvers.reverse('build_annotations:edit_annotations',
                               args=[self._build_id]))
    else:
      self._PopulateContext()
      return shortcuts.render(request, self.template_name, self._context)

  def _PopulateContext(self):
    build_row = self._GetBuildRow()
    if build_row is None:
      raise http.Http404

    self._context = {}
    self._context['build_row'] = build_row
    self._context['annotations_formset'] = self._GetAnnotationsFormSet()

  def _GetBuildRow(self):
    controller = build_row_controller.BuildRowController()
    build_row_list = controller.GetStructuredBuilds(
        latest_build_id=self._build_id,
        num_builds=1)
    if not build_row_list:
      return None
    return build_row_list[0]

  def _GetAnnotationsFormSet(self):
    if self._formset is None:
      build_row = self._GetBuildRow()
      if build_row is not None:
        queryset = build_row.annotations
      else:
        queryset = ba_models.AnnotationsTable.objects.none()
      self._formset = ba_forms.AnnotationsFormSet(queryset=queryset)
    return self._formset

  def _SaveAnnotations(self):
    models_to_save = self._formset.save(commit=False)
    build_row = self._GetBuildRow()
    for model in models_to_save:
      if not hasattr(model, 'build_id') or model.build_id is None:
        model.build_id = build_row.build_entry
        model.last_annotator = self._annotator
      model.save()
