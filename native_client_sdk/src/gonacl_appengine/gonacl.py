import webapp2


application = webapp2.WSGIApplication([
  webapp2.Route('/', webapp2.RedirectHandler, defaults={
    '_uri': 'http://developers.google.com/native-client/'}),
], debug=True)
