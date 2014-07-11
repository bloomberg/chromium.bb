{
  'variables': {
     # The generated paths gen/ui/ui_resources and gen/ui/ui_strings are
     # being moved to gen/ui/resources and gen/ui/strings, respectively. But
     # Blink relies on the location of ui_resources to link its unit tests.
     # Two facilitate the two-sided patch, this variable indicates the
     # one currently in use.
     # TODO(brettw) remove this when the move is complete.
     'ui_resources_gen_subdir': 'resources',
  },
}
