from recipe_engine.recipe_api import Property

DEPS = [
  'gclient',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/raw_io',
  'rietveld',
  'recipe_engine/step',
]

PROPERTIES = {
  'deps_revision_overrides': Property(default={}),
  'event_patchSet_ref': Property(param_name="event_patchSet_ref", default=None),
  'issue': Property(default=None),
  'fail_patch': Property(default=False),
  'failure_type': Property(default=None),
  'parent_got_revision': Property(default=None),
  'patchset': Property(default=None),
  'patch_url': Property(default=None),
  'rietveld': Property(default=None),
  'repository': Property(default=None),
  'revision': Property(default=None),
}
