{
  'DISABLE_PACKAGE': True,  # Doesn't work in packaged apps yet.
  'TOOLS': ['newlib'],
  'SEARCH': [
    '.',
    '../..',
  ],
  'TARGETS': [
    {
      'NAME' : 'debugging',
      'TYPE' : 'main',
      'SOURCES' : [
        'hello_world.c',
      ],
      'CCFLAGS': ['-fno-omit-frame-pointer'],
      'DEPS' : ['error_handling'],
      'LIBS' : ['ppapi', 'pthread']
    }
  ],

  'POST': """
#
# Specify the MAP files to be created.
#
$(eval $(call MAP_RULE,$(TARGET),$(TARGET)))
""",
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/tutorial',
  'NAME': 'debugging',
  'TITLE': 'Debugging',
  'GROUP': 'Tutorial'
}

